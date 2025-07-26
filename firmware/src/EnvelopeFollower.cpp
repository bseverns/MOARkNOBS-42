// Tracks audio or CV levels to modulate MIDI messages.
// Supports SEF and ARG modes and several filter shapes.
// Updated each loop by firmware_main.cpp and consulted by ButtonManager.

#include "EnvelopeFollower.h"
#include "MIDIHandler.h"
#include "BiquadFilter.h"
#include <cmath>

// Tracks external audio/CV and converts it to a MIDI-friendly envelope. The
// value produced here is consumed by PotentiometerManager and the arpeggiator
// to modulate outgoing MIDI data.

/**
 * Constructor
 */
EnvelopeFollower::EnvelopeFollower(int pin, PotentiometerManager* pm)
    : audioInputPin(pin),
      currentEnvelopeLevel(0),
      modulationTargetCC(-1),
      isActive(false),
      filterType(LINEAR),     // initialize filterType first
      mode(SEF),             // then mode
      argMethod(PLUS),
      envelopeA(0),
      envelopeB(1),
      potManager(pm)
{
    // default low-pass at 1kHz
    filter.configure(BiquadFilter::LOWPASS, 1000, 44100, 0.707);
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
 * Convert a raw ADC reading into an envelope value. In SEF mode this applies
 * the selected shaping curve or filter; in ARG mode it combines two external
 * envelopes according to the configured algorithm.
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
                return constrain(pow(level / 127.0f, shapingQ) * (shapingFreq / 1000.0f) * 127.0f, 0, 127);

            case RANDOM: {
                int probability = map(shapingFreq, 20, 5000, 0, 100);
                if (random(0, 100) < probability) {
                    int range = map(shapingQ * 100.0f, 50, 400, 1, 64);
                    return constrain(level + random(-range, range), 0, 127);
                } else {
                    return level;
                }
            }

            default:
                return level;
        }
    } else {
        // ARG mode: read two envelope pins, do your math combos
        int A = 0;
        int B = 0;

        // If envelopeA/B >= 0, treat them as valid analog pins
        if (envelopeA >= 0) {
            int rawA = analogRead(envelopeA);
            A = map(rawA, 0, 1023, 0, 127);
        }
        if (envelopeB >= 0) {
            int rawB = analogRead(envelopeB);
            B = map(rawB, 0, 1023, 0, 127);
        }

        switch (argMethod) {
            case PLUS: return constrain(A + B, 0, 127);
            case MIN:  return constrain(A - B, 0, 127);
            case PECK: return constrain(B - A, 0, 127);
            case SHAV: return constrain((A - B) / 10, 0, 127);
            case SQAR: return constrain((int) sqrt((float)(A * A + B * B)), 0, 127);
            case BABS: return (B != 0) ? constrain(A / abs(B), 0, 127) : 0;
            case TABS: return (B != 0) ? constrain((10 * A) / abs(B), 0, 127) : 0;
            default:   return level;
        }
    }
}

/**
 * update()
 * updates envelope level each loop if active
 */
void EnvelopeFollower::update() {
    if (isActive) {
        int rawLevel = readEnvelopeLevel();
        currentEnvelopeLevel = processEnvelopeLevel(rawLevel);
    }
}

/**
 * applyToCC()
 * final step where envelope modifies CC
 * - Just adds or subtracts the new envelope level
 * - Avoids redundant MIDI messages
 */
// Adjust the given CC value with the current envelope and send it if it changed
// since the last update. This prevents spamming duplicate MIDI messages.
void EnvelopeFollower::applyToCC(int potIndex, uint8_t& ccValue) {
    static uint8_t lastSentCC[NUM_POTS] = {255};

    if (isActive && modulationTargetCC >= 0) {
        int modulatedValue = ccValue + currentEnvelopeLevel;
        ccValue = constrain(modulatedValue, 0, 127);

        if (ccValue != lastSentCC[potIndex]) {
            lastSentCC[potIndex] = ccValue;
            midiHandler.sendControlChange(modulationTargetCC, ccValue, potManager->getChannel(potIndex));
        }
    }
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
bool EnvelopeFollower::getActiveState() const {
    return isActive;
}

/**
 * setModulationTarget()
 */
void EnvelopeFollower::setModulationTarget(int cc) {
    modulationTargetCC = cc;
}

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
EnvelopeFollower::FilterType EnvelopeFollower::getFilterType() const {
    return filterType;
}

/**
 * setMode()
 * - New method for switching between SEF and ARG
 */
void EnvelopeFollower::setMode(Mode newMode) {
    mode = newMode;
}

/**
 * setARGMethod()
 * - New method for selecting among PLUS, MIN, PECK, etc.
 */
void EnvelopeFollower::setARGMethod(ARG_Method method) {
    argMethod = method;
}

/**
 * setEnvelopePair()
 * - New method specifying which two analog inputs to use for A & B in ARG mode
 */
void EnvelopeFollower::setEnvelopePair(int envA, int envB) {
    envelopeA = envA;
    envelopeB = envB;
}

void EnvelopeFollower::calibrateBaseline() {
    const uint8_t samples = 8;
    uint32_t total = 0;
    for (uint8_t i = 0; i < samples; ++i) {
        total += analogRead(audioInputPin);
        delayMicroseconds(10);
    }
    float avg = static_cast<float>(total) / samples;
    baseline = avg * VadcScale - g_vref;
}

/**
 * readEnvelopeLevel()
 * Helper used by update() to read the raw envelope value
 * from the configured analog pin and map it to a MIDI range.
 */
int EnvelopeFollower::readEnvelopeLevel() {
    float env = (analogRead(audioInputPin) * VadcScale - g_vref - baseline) * gain;
    env = max(0.0f, env);
    int midi = static_cast<int>((env / g_vref) * 127.0f);
    return constrain(midi, 0, 127);
}

/**
 * getEnvelopeLevel()
 */
int EnvelopeFollower::getEnvelopeLevel() const {
    return currentEnvelopeLevel;
}
