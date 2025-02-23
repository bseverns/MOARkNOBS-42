#include "EnvelopeFollower.h"
#include "MIDIHandler.h"
#include "BiquadFilter.h"
#include <cmath>

// Keep your external references
extern MIDIHandler midiHandler;

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
 * readEnvelopeLevel()
 * reads from the assigned audio pin,
 *   maps to 0–127 for typical MIDI range.
 */
int EnvelopeFollower::readEnvelopeLevel() {
    int rawValue = analogRead(audioInputPin);
    return map(rawValue, 0, 1023, 0, 127);
}

/**
 * processEnvelopeLevel(int level)
 * - If mode == SEF, it behaves exactly like your original code:
 *   filtering (low, high, band) or curve (linear, opposite, etc.).
 * - If mode == ARG, it uses your math combos (PLUS, MIN, etc.)
 *   reading from envelopeA and envelopeB.
 */
int EnvelopeFollower::processEnvelopeLevel(int level) {
    level = constrain(level, 0, 127);

    // Original envelope follower mode
    if (mode == SEF) {
        // Original filter logic
        if (filterType == LOWPASS || filterType == HIGHPASS || filterType == BANDPASS) {
            return filter.process(level);
        }
        // Original processing logic
        switch (filterType) {
            case LINEAR:
                return level;
            case OPPOSITE_LINEAR:
                return 127 - level;
            case EXPONENTIAL:
                return pow(level / 127.0, 2) * 127;
            case RANDOM:
                return random(level);
            default:
                return level;
        }
    }
    else {
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
void EnvelopeFollower::applyToCC(int potIndex, uint8_t& ccValue) {
    static uint8_t lastSentCC[NUM_POTS] = {255}; // same as original

    if (isActive && modulationTargetCC >= 0) {
        int modulatedValue = ccValue + currentEnvelopeLevel;
        ccValue = constrain(modulatedValue, 0, 127);

        // Original redundancy check
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
            // LINEAR, OPPOSITE_LINEAR, EXPONENTIAL, RANDOM -> no filter usage
            break;
    }
}

/**
 * configureFilter()
 * - Keep the function that was used elsewhere for dynamic changes to freq & Q
 */
void EnvelopeFollower::configureFilter(float frequency, float q) {
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
            // Non-filter types skip
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

/**
 * getEnvelopeLevel()
 */
int EnvelopeFollower::getEnvelopeLevel() const {
    return currentEnvelopeLevel;
}
