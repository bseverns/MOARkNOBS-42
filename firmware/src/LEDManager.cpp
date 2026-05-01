// LEDManager is the lightshow conductor. The tone here is intentionally chatty
// so builders understand how DMA-backed strips, dirty-flag batching, and
// colour math collide. Follow along to see how we keep the visuals responsive
// without burning CPU cycles.

#include "LEDManager.h"
#include "Globals.h" // hardware config
#include "TimeUtils.h"
#include <FastLED.h>
#include <algorithm>
#include <map>
#include <string>

LEDManager::~LEDManager() {
    // Nothing to delete; STL containers clean up automatically
}

LEDManager::LEDManager(const HardwareConfig &config)
    : cfg(config), numLEDs(NUM_LEDS()), modeDisplay(0), activePot(255), envelopeModeActive(false),
      brightness(MN42_DEFAULT_LED_BRIGHTNESS), currentState(LEDState::IDLE), activeIndex(255) {
    leds.resize(numLEDs);
    dirtyFlags.resize(numLEDs, false);

    laneLength = numLEDs;
    constexpr size_t kLaneCount = 8;
    octoLanes.resize(static_cast<size_t>(laneLength) * kLaneCount, CRGB::Black);

    std::fill(leds.begin(), leds.end(), CRGB::Black);
}

// Legacy begin hook retained so callers can treat LEDManager like other hardware modules.
void LEDManager::begin() {
    if (initialized) {
        return;
    }

    // OctoWS2811 fans data across eight parallel lanes. Only lane 4 is wired
    // in this rig, but we keep the other slots zeroed so the DMA bus stays
    // quiet.
    FastLED.addLeds<OCTOWS2811>(octoLanes.data(), laneLength).setCorrection(TypicalLEDStrip);
    initialized = true;
    // Always cap startup brightness conservatively on first bring-up.
    if (brightness > MN42_DEFAULT_LED_BRIGHTNESS) {
        brightness = MN42_DEFAULT_LED_BRIGHTNESS;
    }
    applyBrightness();
    presentFrame();
}

// Paint one pot lane from its current MIDI value.
void LEDManager::setPotValue(uint8_t potIndex, uint8_t value) {
    if (potIndex < leds.size()) {
        leds[potIndex] = CHSV(map(value, 0, 127, 0, 255), 255, 255);
        markDirty(potIndex);
    }
}

// Paint one envelope lane from its current follower level.
void LEDManager::setEnvelopeLevel(uint8_t efIndex, uint8_t value) {
    uint16_t idx = EF_LED_OFFSET() + efIndex;
    if (idx < leds.size()) {
        uint8_t b = map(value, 0, 127, 0, 255);
        leds[idx] = CRGB(b, b, b);
        markDirty(idx);
    }
}

// Paint the dedicated pot-indicator LEDs that mirror current control values.
void LEDManager::setPotIndicator(uint8_t potIndex, uint8_t value) {
    uint16_t idx = POT_LED_OFFSET() + potIndex;
    if (idx < leds.size()) {
        leds[idx] = CHSV(map(value, 0, 127, 0, 255), 255, 255);
        markDirty(idx);
    }
}

// Trigger the short control-button flash animation.
void LEDManager::triggerControlButton() {
    controlStart = now();
    controlActive = true;
}

// Show the currently active mode on the strip.
void LEDManager::setModeDisplay(uint8_t mode) {
    modeDisplay = mode;
    for (size_t i = 0; i < leds.size(); i++) {
        leds[i] = (i == mode) ? CRGB::Blue : CRGB::Black;
        markDirty(i);
    }
}

// Highlight the active pot and clear the previous highlight.
void LEDManager::setActivePot(uint8_t potIndex) {
    if (activePot < leds.size()) {
        leds[activePot] = CRGB::Black;
        markDirty(activePot);
    }
    activePot = potIndex;
    if (activePot < leds.size()) {
        leds[activePot] = CRGB::Red;
        markDirty(activePot);
    }
}

// Switch the strip into or out of the envelope-mode visual state.
void LEDManager::indicateEnvelopeMode(bool isActive) {
    envelopeModeActive = isActive;
    currentState = isActive ? LEDState::ENVELOPE_MODE : LEDState::IDLE;
    for (size_t i = 0; i < leds.size(); i++) {
        leds[i] = isActive ? CRGB::Green : leds[i];
        markDirty(i);
    }
}

// Mark one pixel dirty so `update()` knows it needs to be pushed.
void LEDManager::markDirty(uint8_t index) {
    if (index < dirtyFlags.size()) {
        dirtyFlags[index] = true;
    }
}

// Apply a new global brightness immediately.
void LEDManager::setBrightness(uint8_t b) {
    brightness = b;
    applyBrightness();
    presentFrame();
}

// Apply a multiplicative brightness modulator for animation and diagnostics.
void LEDManager::setBrightnessModulator(float scale) {
    // Clamp modulation so brightness stays in a safe range.
    if (scale < 0.0f)
        brightnessMod = 0.0f;
    else if (scale > 2.0f)
        brightnessMod = 2.0f;
    else
        brightnessMod = scale;
    applyBrightness();
}

// Return the current brightness modulation factor.
float LEDManager::getBrightnessModulator() const { return brightnessMod; }

// Flood the strip with one color and push the frame immediately.
void LEDManager::setColor(CRGB color) {
    for (size_t i = 0; i < leds.size(); i++) {
        leds[i] = color;
        markDirty(i);
    }
    presentFrame();
}

// Return the base strip brightness.
uint8_t LEDManager::getBrightness() const { return brightness; }

// Return the first LED color as the strip's representative stored color.
CRGB LEDManager::getColor() const { return leds.empty() ? CRGB::Black : leds[0]; }

/**
 * @brief Runs a white sweep while the box boots.
 *
 * Each LED gets ~20 ms of fame before going dark again. It's a blocking
 * love letter to the strip, so the rest of the firmware sits tight until
 * the dance is over.
 */
void LEDManager::startupAnimation() {
    for (size_t i = 0; i < leds.size(); i++) {
        leds[i] = CRGB::White;
        presentFrame();
        delay(20);
        leds[i] = CRGB::Black;
        markDirty(i);
    }
    presentFrame();
}

// Swap the strip state machine and repaint around the newly active index.
void LEDManager::setState(LEDState state, uint8_t index) {
    currentState = state;
    activeIndex = index;
    update();
}

/**
 * @brief Treats the strip as one big unruly group and splashes a single colour on it.
 *
 * Call when you want a full wipe. Every pixel gets tagged via @ref dirtyFlags so a
 * later update() knows who changed. We also push the frame immediately so the
 * strip mirrors the new colour without waiting for loop().
 *
 * @param color The hue to paint across the entire strip.
 */
void LEDManager::setAll(const CRGB &color) {
    for (auto &led : leds) {
        led = color;
        // Taking the pointer difference here converts the reference back into an
        // index. It’s the most direct way to flag the right LED without juggling
        // counters alongside the range-for loop.
        markDirty(&led - &leds[0]);
    }
    presentFrame();
}

void LEDManager::setPixelColor(uint16_t index, const CRGB &color) {
    if (index >= leds.size())
        return;
    leds[index] = color;
    markDirty(static_cast<uint8_t>(index));
}

/**
 * @brief Repaints a named LED group in one shot.
 *
 * Groups are string keys mapped to index lists—think crews like "pots" or
 * "buttons". Use this when a whole crew needs a new vibe. Each member's
 * @ref dirtyFlags entry is set so update() can flush them together, though we
 * also force an immediate refresh.
 *
 * @param group The posse to recolour.
 * @param color Fresh paint for the group.
 */
void LEDManager::setGroupColor(const std::string &group, const CRGB &color) {
    auto it = ledGroups.find(group);
    if (it == ledGroups.end())
        return;
    for (uint16_t idx : it->second) {
        leds[idx] = color;
        markDirty(idx);
    }
    presentFrame();
}

void LEDManager::beginWarningAnimation(LEDWarning type) {
    warningType = type;
    warningActive = (type != LEDWarning::None);
    warningStart = now();
}

// Stop any warning animation and clear its bookkeeping state.
void LEDManager::clearWarningAnimation() {
    if (!warningActive)
        return;
    warningActive = false;
    warningType = LEDWarning::None;
    presentFrame();
    std::fill(dirtyFlags.begin(), dirtyFlags.end(), false);
}

/**
 * @brief Pushes any dirty LEDs out to the strip and clears the flags.
 *
 * Functions like setAll() and setGroupColor() flip @ref dirtyFlags for the
 * pixels they touch. Call update() from the main loop after staging those
 * changes; it shows the new frame and resets all flags so the next round can
 * track fresh edits.
 */
void LEDManager::update() {
    if (controlActive) {
        unsigned long elapsed = now() - controlStart;
        if (elapsed < 750) {
            leds[CONTROL_LED_INDEX()] = CRGB::White;
        } else if (elapsed < 2000) {
            leds[CONTROL_LED_INDEX()] = CRGB(127, 127, 127);
        } else {
            leds[CONTROL_LED_INDEX()] = CRGB::Black;
            controlActive = false;
        }
        markDirty(CONTROL_LED_INDEX());
    }

    if (diagnosticMode && leds.size() >= 4) {
        uint8_t idx = leds.size() - 4;
        uint8_t b = sin8((now() - diagStart) >> 2);
        leds[idx] = CRGB(b, b, b);
        markDirty(idx);
    }

    switch (currentState) {
    case LEDState::ACTIVE_POT:
        if (activeIndex < leds.size())
            leds[activeIndex] = CRGB::Red;
        break;
    case LEDState::ENVELOPE_MODE:
        for (auto &led : leds)
            led = CRGB::Green;
        break;
    case LEDState::ARG_MODE:
        if (activeIndex < leds.size())
            leds[activeIndex] = CRGB::Blue;
        break;
    case LEDState::MIDI_UPDATE:
        if (activeIndex < leds.size())
            leds[activeIndex] = CRGB::Yellow;
        break;
    case LEDState::TEMP_FEEDBACK:
        if (activeIndex < leds.size())
            leds[activeIndex] = CRGB::White;
        break;
    case LEDState::IDLE:
    default:
        break; // keep existing colours
    }

    if (warningActive) {
        renderWarningFrame();
        std::fill(dirtyFlags.begin(), dirtyFlags.end(), false);
        return;
    }

    presentFrame();
    std::fill(dirtyFlags.begin(), dirtyFlags.end(), false);
}

void LEDManager::syncToOctoBuffer() {
    if (octoLanes.empty())
        return;

    std::fill(octoLanes.begin(), octoLanes.end(), CRGB::Black);

    if (laneLength == 0)
        return;

    const size_t copyCount = std::min(static_cast<size_t>(laneLength), leds.size());
    CRGB *laneBase = octoLanes.data() + static_cast<size_t>(kOctoPinLane) * laneLength;
    std::copy_n(leds.begin(), copyCount, laneBase);
}

void LEDManager::presentFrame() {
    if (!initialized)
        return;
    syncToOctoBuffer();
    FastLED.show();
}

void LEDManager::applyBrightness() {
    if (!initialized)
        return;
    // Convert base brightness plus modulation into a FastLED brightness value.
    float scaled = static_cast<float>(brightness) * brightnessMod;
    if (scaled < 0.0f)
        scaled = 0.0f;
    if (scaled > 255.0f)
        scaled = 255.0f;
    FastLED.setBrightness(static_cast<uint8_t>(scaled));
}

void LEDManager::renderWarningFrame() {
    if (!initialized)
        return;
    if (octoLanes.empty())
        return;

    std::fill(octoLanes.begin(), octoLanes.end(), CRGB::Black);

    if (laneLength == 0)
        return;

    const size_t copyCount = std::min(static_cast<size_t>(laneLength), leds.size());
    CRGB *laneBase = octoLanes.data() + static_cast<size_t>(kOctoPinLane) * laneLength;
    unsigned long elapsed = now() - warningStart;

    switch (warningType) {
    case LEDWarning::Destructive: {
        bool strobeOn = ((elapsed / 120) % 2) == 0;
        if (strobeOn) {
            for (size_t i = 0; i < copyCount; ++i) {
                laneBase[i] = (i % 2 == 0) ? CRGB::Red : CRGB::White;
            }
        }
        break;
    }
    case LEDWarning::Diagnostic: {
        uint16_t phase = static_cast<uint16_t>((elapsed / 40) % 256);
        for (size_t i = 0; i < copyCount; ++i) {
            uint8_t brightness = sin8(static_cast<uint8_t>(phase + i * 8));
            laneBase[i] = CHSV(160, 200, brightness);
        }
        break;
    }
    case LEDWarning::None:
    default:
        break;
    }

    FastLED.show();
}

void LEDManager::setStatusLED(bool on) { digitalWrite(cfg.statusLedPin, on ? HIGH : LOW); }

void LEDManager::blinkStatusLED(uint8_t times, uint16_t delayMs) {
    for (uint8_t i = 0; i < times; ++i) {
        setStatusLED(true);
        delay(delayMs);
        setStatusLED(false);
        delay(delayMs);
    }
}

void LEDManager::setDiagnosticMode(bool enabled) {
    diagnosticMode = enabled;
    diagStart = now();
    if (!enabled && leds.size() >= 4) {
        leds[leds.size() - 4] = CRGB::Black;
        markDirty(leds.size() - 4);
        presentFrame();
    }
}
