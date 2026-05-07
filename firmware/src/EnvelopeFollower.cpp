// EnvelopeFollower turns raw analog voltage into musically useful control data.
// This file spells out every step—how pins get resolved, how filters sculpt the
// signal, and why different modes take different math routes. Read it if you’re
// trying to teach someone the difference between "sampling" and "shaping" or
// why we stash filter parameters alongside the object instead of in global soup.

#include "EnvelopeFollower.h"
#include "BiquadFilter.h"
#include "ConfigManager.h"
#include "Globals.h"
#include "Hardware/IO.h"
#include "PerlinNoise.h"
#include "TimeUtils.h"
#include <cmath>
#include <algorithm>

// Tracks external audio/CV and converts it to a MIDI-friendly envelope. The
// value produced here is consumed by PotentiometerManager and the arpeggiator
// to modulate outgoing MIDI data.

/**
 * Constructor
 */
namespace {
// Normalize caller-provided envelope identifiers into actual analog pin numbers.
int resolveEnvelopeInput(int candidate) {
    if (candidate < 0) {
        return candidate;
    }

    // Already an analog pin? map back to index and return canonical pin value.
    int idx = envelopeIndexFromAnalogPin(candidate);
    if (idx >= 0) {
        return ENVELOPE_ANALOG_PINS[static_cast<size_t>(idx)];
    }

    // Treat small integers as envelope indices (0..NUM_ENVELOPES-1).
    if (candidate < NUM_ENVELOPES) {
        int analog = envelopeAnalogPin(static_cast<uint8_t>(candidate));
        if (analog >= 0) {
            return analog;
        }
    }

    return candidate;
}

// Convert the global jitter smoothness control into a per-step Perlin sample rate.
float jitterRateFromSmoothness(float smoothness) {
    float clamped = constrain(smoothness, 0.0f, 1.0f);
    return 0.05f + (1.0f - clamped) * 1.95f;
}

// Read the global jitter depth knob with a safe clamp.
float jitterDepth() {
    float base = constrain(g_jitterSettings.depth, 0.0f, 1.0f);
    return constrain(base + (g_lfoJitterDepth * 0.5f), 0.0f, 1.0f);
}

float jitterSmoothness() {
    float base = constrain(g_jitterSettings.smoothness, 0.0f, 1.0f);
    return constrain(base + (g_lfoJitterSmoothness * 0.5f), 0.0f, 1.0f);
}
} // namespace

/**
 * Convert the serialized EF filter enum into the runtime filter selection.
 * Keeping this one place prevents subtle drift when we add new filters later.
 */
EnvelopeFollower::FilterType
EnvelopeFollower::filterFromEfType(MIDISlot::EfSettings::FilterType type) {
    switch (type) {
    case MIDISlot::EfSettings::FilterType::Linear:
        return EnvelopeFollower::LINEAR;
    case MIDISlot::EfSettings::FilterType::OppositeLinear:
        return EnvelopeFollower::OPPOSITE_LINEAR;
    case MIDISlot::EfSettings::FilterType::Exponential:
        return EnvelopeFollower::EXPONENTIAL;
    case MIDISlot::EfSettings::FilterType::Random:
        return EnvelopeFollower::RANDOM;
    case MIDISlot::EfSettings::FilterType::Lowpass:
        return EnvelopeFollower::LOWPASS;
    case MIDISlot::EfSettings::FilterType::Highpass:
        return EnvelopeFollower::HIGHPASS;
    case MIDISlot::EfSettings::FilterType::Bandpass:
        return EnvelopeFollower::BANDPASS;
    }
    return EnvelopeFollower::LINEAR;
}

EnvelopeFollower::EnvelopeFollower(int pin, PotentiometerManager *pm, uint8_t id)
    : audioInputPin(pin), index(id), currentEnvelopeLevel(0), modulationTargetCC(-1),
      isActive(false), filterType(LINEAR), mode(SEF), argMethod(PLUS),
      envelopeA(resolveEnvelopeInput(0)), envelopeB(resolveEnvelopeInput(1)), vref(g_vref),
      potManager(pm) {
    // default low-pass at 1kHz
    filter.configure(BiquadFilter::LOWPASS, 1000, 44100, 0.707);
    efSettings.mode = efMode;
}

/**
 * configureFilter()
 * - Keep the function that was used elsewhere for dynamic changes to freq & Q
 */
void EnvelopeFollower::configureFilter(float frequency, float q) {
    shapingFreq = frequency;
    shapingQ = q;

    switch (filterType) {
    case LOWPASS:
        filter.configure(BiquadFilter::LOWPASS, frequency, 44100, q);
        break;
    case HIGHPASS:
        filter.configure(BiquadFilter::HIGHPASS, frequency, 44100, q);
        break;
    case BANDPASS:
        filter.configure(BiquadFilter::BANDPASS, frequency, 44100, q);
        break;
    default:
        // parameters stored directly for other modes
        break;
    }
}

/**
 * Convert a raw ADC reading into an envelope value using the configured filter
 * or shaping curve.
 */
int EnvelopeFollower::processEnvelopeLevel(int level) {
    level = constrain(level, 0, 127);

    if (mode == SEF) {
        if (filterType == LOWPASS || filterType == HIGHPASS || filterType == BANDPASS) {
            return filter.process(level);
        }

        switch (filterType) {
        case LINEAR:
            return constrain(level * (shapingFreq / 1000.0f), 0, 127);

        case OPPOSITE_LINEAR:
            return constrain(127 - (level * (shapingFreq / 1000.0f)), 0, 127);

        case EXPONENTIAL:
            return constrain(pow(level / 127.0f, shapingQ) * (shapingFreq / 1000.0f) * 127.0f, 0,
                             127);

        case RANDOM: {
            int probability = map(shapingFreq, 20, 5000, 0, 100);
            if (random(0, 100) < probability) {
                int range = map(shapingQ * 100.0f, 50, 400, 1, 64);
                float depth = jitterDepth();
                if (depth <= 0.0f) {
                    return level;
                }
                float rate = jitterRateFromSmoothness(jitterSmoothness());
                float t = (static_cast<float>(now()) * 0.001f * rate) + (index * 13.37f);
                float n = perlinNoise1D(t);
                int swing = static_cast<int>(roundf(n * static_cast<float>(range) * depth));
                return constrain(level + swing, 0, 127);
            } else {
                return level;
            }
        }

        default:
            return level;
        }
    }

    // ARG mode: grab the paired envelopes, map to MIDI range, then blend.
    int A = 0;
    int B = 0;

    if (envelopeA >= 0) {
        int rawA = hardware::readAnalog(envelopeA);
        A = map(rawA, 0, 1023, 0, 127);
    }
    if (envelopeB >= 0) {
        int rawB = hardware::readAnalog(envelopeB);
        B = map(rawB, 0, 1023, 0, 127);
    }

    switch (argMethod) {
    case PLUS:
        return constrain(A + B, 0, 127);
    case MIN:
        return constrain(A - B, 0, 127);
    case PECK:
        return constrain(B - A, 0, 127);
    case SHAV:
        return constrain((A - B) / 10, 0, 127);
    case SQAR:
        return constrain(static_cast<int>(sqrt(static_cast<float>(A * A + B * B))), 0, 127);
    case BABS:
        return (B != 0) ? constrain(A / abs(B), 0, 127) : 0;
    case TABS:
        return (B != 0) ? constrain((10 * A) / abs(B), 0, 127) : 0;
    case MULT:
        return constrain((A * B) / 127, 0, 127);
    case DIVI:
        return constrain((A * 127) / (B + 1), 0, 127);
    case AVG:
        return constrain((A + B) / 2, 0, 127);
    case XABS:
        return constrain(abs(A - B), 0, 127);
    case MAXX:
        return max(A, B);
    case MINN:
        return min(A, B);
    case XORR:
        return (A ^ B) & 0x7F;
    }
    return constrain(A + B, 0, 127);
}

/**
 * update()
 * updates envelope level each loop if active
 */
void EnvelopeFollower::update() {
    if (!isActive) {
        return;
    }
    unsigned long nowMs = now();
    float dt = 0.0f;
    if (lastUpdateMs != 0 && nowMs >= lastUpdateMs) {
        dt = static_cast<float>(nowMs - lastUpdateMs) / 1000.0f;
    }
    lastUpdateMs = nowMs;
    if (dt <= 0.0f) {
        dt = 0.005f; // default to the mid-priority scheduler cadence
    }
    int rawLevel = readEnvelopeLevel(dt);
    currentEnvelopeLevel = applyIdleFloor(processEnvelopeLevel(rawLevel));
}

/**
 * applyToCC()
 * final step where envelope modifies CC
 * - Just adds or subtracts the new envelope level
 * - Avoids redundant MIDI messages
 */
// Adjust the given CC value with the current envelope. The caller is
// responsible for deciding whether to transmit the updated value.
void EnvelopeFollower::applyToCC(int potIndex, uint8_t &ccValue) {
    static_cast<void>(potIndex); // Pot index kept for future per-slot tweaks

    if (!isActive)
        return;

    int modulatedValue = ccValue + currentEnvelopeLevel;
    ccValue = constrain(modulatedValue, 0, 127);
}

/**
 * toggleActive()
 */
void EnvelopeFollower::toggleActive(bool state) {
    if (isActive != state) {
        isActive = state;
    }
}

/**
 * getActiveState()
 */
bool EnvelopeFollower::getActiveState() const { return isActive; }

/**
 * setModulationTarget()
 */
void EnvelopeFollower::setModulationTarget(int cc) { modulationTargetCC = cc; }

/**
 * setFilterType()
 */
void EnvelopeFollower::setFilterType(FilterType type) {
    filterType = type;
    // Reapply default config based on new filter type
    switch (type) {
    case LOWPASS:
        filter.configure(BiquadFilter::LOWPASS, 1000, 44100, 0.707);
        break;
    case HIGHPASS:
        filter.configure(BiquadFilter::HIGHPASS, 1000, 44100, 0.707);
        break;
    case BANDPASS:
        filter.configure(BiquadFilter::BANDPASS, 1000, 44100, 0.707);
        break;
    default:
        break;
    }
}

/**
 * getFilterType()
 */
EnvelopeFollower::FilterType EnvelopeFollower::getFilterType() const { return filterType; }

float EnvelopeFollower::getShapingFrequency() const { return shapingFreq; }

float EnvelopeFollower::getShapingQ() const { return shapingQ; }

void EnvelopeFollower::setMode(Mode newMode) { mode = newMode; }

void EnvelopeFollower::setMode(EFMode newMode) {
    // Switching modes resets the detector state so transitions feel clean.
    if (efMode == newMode) {
        return;
    }
    efMode = newMode;
    efSettings.mode = newMode;
    peakState = 0.0f;
    rmsState = 0.0f;
    gateOpen = false;
}

void EnvelopeFollower::setExternalGainTrim(float trim) {
    // External trim scales the computed level (used by modulation sources).
    if (!std::isfinite(trim)) {
        return;
    }
    externalGainTrim = constrain(trim, 0.0f, 2.0f);
}

void EnvelopeFollower::setModeSettings(const EfModeSettings &settings) {
    // Keep the stored settings in sync with the active mode.
    efSettings = settings;
    setMode(settings.mode);
    efSettings = settings;
}

void EnvelopeFollower::configureFromEfSettings(const MIDISlot::EfSettings &settings) {
    // One crystal-clear path for restoring persisted EF tuning keeps the rest
    // of the firmware from re-implementing these assignments in a dozen places.
    setFilterType(filterFromEfType(settings.filterType));
    configureFilter(settings.frequency, settings.q);
    setOversampleCount(settings.oversample);
    setSmoothingAlpha(settings.smoothing);
    setBaseline(settings.baseline);
    setGain(settings.gain);
    EfModeSettings modeSettings{};
    modeSettings.mode = static_cast<EFMode>(settings.efMode);
    modeSettings.attackMs = settings.attackMs;
    modeSettings.releaseMs = settings.releaseMs;
    modeSettings.rmsWindowMs = settings.rmsWindowMs;
    modeSettings.baselineTauMs = settings.baselineTauMs;
    modeSettings.gainTauMs = settings.gainTauMs;
    modeSettings.gateThreshold = settings.gateThreshold;
    modeSettings.gateHysteresis = settings.gateHysteresis;
    modeSettings.activityThreshold = settings.activityThreshold;
    modeSettings.gainTarget = settings.gainTarget;
    modeSettings.autoBaseline = settings.autoBaseline != 0;
    modeSettings.autoGain = settings.autoGain != 0;
    setModeSettings(modeSettings);
}

EnvelopeFollower::EfModeSettings EnvelopeFollower::getModeSettings() const { return efSettings; }

EnvelopeFollower::EfStats EnvelopeFollower::getStats() const {
    // Snapshot the current baseline, gain, output, and mode for diagnostics.
    EfStats stats;
    stats.baseline = baseline;
    stats.gain = gainScale();
    stats.value = static_cast<float>(currentEnvelopeLevel);
    stats.mode = static_cast<uint8_t>(efMode);
    return stats;
}

void EnvelopeFollower::setARGMethod(ARG_Method method) { argMethod = method; }

EnvelopeFollower::ARG_Method EnvelopeFollower::getARGMethod() const { return argMethod; }

void EnvelopeFollower::setEnvelopePair(int envA, int envB) {
    envelopeA = resolveEnvelopeInput(envA);
    envelopeB = resolveEnvelopeInput(envB);
}

int EnvelopeFollower::getEnvelopeA() const { return envelopeA; }

int EnvelopeFollower::getEnvelopeB() const { return envelopeB; }

void EnvelopeFollower::calibrate() {
    const uint8_t samples = 8;
    uint32_t refTotal = 0;
    for (uint8_t i = 0; i < samples; ++i) {
        refTotal += hardware::readAnalog(VREF_ADC_PIN);
        delayMicroseconds(10);
    }
    vref = (static_cast<float>(refTotal) / samples) * VadcScale;
    calibrateBaseline();
    configManager.saveEnvelopeBaseline(index, baseline);
}

void EnvelopeFollower::calibrateBaseline() {
    const uint8_t samples = 8;
    uint32_t total = 0;
    for (uint8_t i = 0; i < samples; ++i) {
        total += hardware::readAnalog(audioInputPin);
        delayMicroseconds(10);
    }
    float avg = static_cast<float>(total) / samples;
    baseline = avg * VadcScale - vref;
}

void EnvelopeFollower::setOversampleCount(uint8_t count) { oversampleCount = count ? count : 1; }

uint8_t EnvelopeFollower::getOversampleCount() const { return oversampleCount; }

void EnvelopeFollower::setSmoothingAlpha(float alpha) {
    smoothingAlpha = constrain(alpha, 0.0f, 1.0f);
}

float EnvelopeFollower::getSmoothingAlpha() const { return smoothingAlpha; }

/**
 * readEnvelopeLevel()
 * Helper used by update() to read the raw envelope value
 * from the configured analog pin and map it to a MIDI range.
 */
int EnvelopeFollower::readEnvelopeLevel(float dtSeconds) {
    // Oversample the ADC to reduce noise and promote stability.
    uint32_t total = 0;
    for (uint8_t i = 0; i < oversampleCount; ++i) {
        total += hardware::readAnalog(audioInputPin);
        delayMicroseconds(10);
    }
    float avg = static_cast<float>(total) / oversampleCount;
    float sample = avg * VadcScale - vref;
    float rectified = std::max(0.0f, sample);

    // Update baseline and gain before applying the detector mode.
    updateAutoBaseline(rectified, dtSeconds);
    float adjusted = rectified - baseline;
    if (adjusted < 0.0f) {
        adjusted = 0.0f;
    }

    float level = modeOutput(adjusted, dtSeconds);
    float gainApplied = updateAutoGain(level, dtSeconds);

    // Scale to MIDI range and apply smoothing.
    float env = level * gainApplied;
    env = std::max(0.0f, env);
    int midi = static_cast<int>((env / vref) * 127.0f);
    smoothedLevel = smoothingAlpha * midi + (1.0f - smoothingAlpha) * smoothedLevel;
    return applyIdleFloor(constrain(smoothedLevel, 0, 127));
}

float EnvelopeFollower::detectPeak(float level, float dtSeconds) {
    // Peak detector with separate attack/release time constants.
    float attackMs = static_cast<float>(std::max<uint16_t>(1, efSettings.attackMs));
    float releaseMs = static_cast<float>(std::max<uint16_t>(1, efSettings.releaseMs));
    float attackCoeff = 1.0f - expf(-dtSeconds / (attackMs / 1000.0f));
    float releaseCoeff = 1.0f - expf(-dtSeconds / (releaseMs / 1000.0f));
    float coeff = (level > peakState) ? attackCoeff : releaseCoeff;
    peakState += (level - peakState) * coeff;
    return peakState;
}

float EnvelopeFollower::detectRms(float level, float dtSeconds) {
    // RMS-ish detector: integrate squared samples then sqrt.
    float windowMs = static_cast<float>(std::max<uint16_t>(1, efSettings.rmsWindowMs));
    float coeff = 1.0f - expf(-dtSeconds / (windowMs / 1000.0f));
    float squared = level * level;
    rmsState += (squared - rmsState) * coeff;
    return sqrtf(std::max(0.0f, rmsState));
}

float EnvelopeFollower::detectGate(float level) {
    // Gate with hysteresis to prevent chatter around the threshold.
    float threshold = (static_cast<float>(efSettings.gateThreshold) / 127.0f) * vref;
    float hysteresis = (static_cast<float>(efSettings.gateHysteresis) / 127.0f) * vref;
    if (gateOpen) {
        if (level < (threshold - hysteresis)) {
            gateOpen = false;
        }
    } else if (level > (threshold + hysteresis)) {
        gateOpen = true;
    }
    return gateOpen ? vref : 0.0f;
}

float EnvelopeFollower::detectFollower(float level, float dtSeconds) {
    // Fast attack/release follower (shares the peakState integrator).
    float attackMs = static_cast<float>(std::max<uint16_t>(1, efSettings.attackMs));
    float releaseMs = static_cast<float>(std::max<uint16_t>(1, efSettings.releaseMs));
    float attackCoeff = 1.0f - expf(-dtSeconds / (attackMs / 1000.0f));
    float releaseCoeff = 1.0f - expf(-dtSeconds / (releaseMs / 1000.0f));
    float coeff = (level > peakState) ? attackCoeff : releaseCoeff;
    peakState += (level - peakState) * coeff;
    return peakState;
}

void EnvelopeFollower::updateAutoBaseline(float inputLevel, float dtSeconds) {
    // Track baseline slowly when signal is idle; freeze during activity.
    if (!efSettings.autoBaseline) {
        return;
    }
    float activityThreshold = (static_cast<float>(efSettings.activityThreshold) / 127.0f) * vref;
    if (inputLevel >= activityThreshold) {
        return;
    }
    float tauMs = static_cast<float>(std::max<uint16_t>(1, efSettings.baselineTauMs));
    float coeff = 1.0f - expf(-dtSeconds / (tauMs / 1000.0f));
    baseline += (inputLevel - baseline) * coeff;
}

float EnvelopeFollower::updateAutoGain(float inputLevel, float dtSeconds) {
    // Adjust gain to hit target level when signal is active and clean.
    float effectiveGain = gainScale();
    if (!efSettings.autoGain) {
        return effectiveGain;
    }

    float activityThreshold = (static_cast<float>(efSettings.activityThreshold) / 127.0f) * vref;
    if (inputLevel < activityThreshold || baseline > activityThreshold) {
        return effectiveGain;
    }

    float target = (static_cast<float>(efSettings.gainTarget) / 127.0f) * vref;
    if (target <= 0.0f) {
        return effectiveGain;
    }

    float current = inputLevel * effectiveGain;
    if (current >= vref * 0.98f) {
        return effectiveGain;
    }

    // Solve for gain that would hit the target, then converge slowly.
    float desired = target / std::max(0.001f, inputLevel * gain * externalGainTrim);
    float tauMs = static_cast<float>(std::max<uint16_t>(1, efSettings.gainTauMs));
    float coeff = 1.0f - expf(-dtSeconds / (tauMs / 1000.0f));
    autoGain += (desired - autoGain) * coeff;
    autoGain = constrain(autoGain, 0.25f, 4.0f);
    return gainScale();
}

float EnvelopeFollower::modeOutput(float inputLevel, float dtSeconds) {
    // Dispatch to the correct detection algorithm for the current mode.
    switch (efMode) {
    case EFMode::RMS:
        return detectRms(inputLevel, dtSeconds);
    case EFMode::Gate:
        return detectGate(inputLevel);
    case EFMode::Follower:
        return detectFollower(inputLevel, dtSeconds);
    case EFMode::Peak:
    default:
        return detectPeak(inputLevel, dtSeconds);
    }
}

float EnvelopeFollower::gainScale() const {
    // Total scale is the product of manual gain, external trim, and auto gain.
    float scale = gain * externalGainTrim * autoGain;
    if (scale < 0.0f) {
        scale = 0.0f;
    }
    return scale;
}

int EnvelopeFollower::applyIdleFloor(int midiLevel) {
    const int configuredFloor = static_cast<int>(efSettings.activityThreshold);
    const int floor = std::max(configuredFloor, static_cast<int>(g_efIdleFloor));
    if (midiLevel <= floor) {
        smoothedLevel = 0;
        return 0;
    }
    return constrain(midiLevel, 0, 127);
}

/**
 * getEnvelopeLevel()
 */
int EnvelopeFollower::getEnvelopeLevel() const { return currentEnvelopeLevel; }
