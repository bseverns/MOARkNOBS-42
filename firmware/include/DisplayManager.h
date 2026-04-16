// Provides simple wrappers around the OLED display for status output.
// Updated by ButtonManager, MIDIHandler and the main loop.
// Manages startup animation and idle screensaver.
#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <array>
#include <cstddef>
#include "Globals.h" // for OLED_WIDTH, OLED_HEIGHT

struct ButtonManagerContext;
class ButtonManager;
class MIDIHandler;
struct SystemDiagnostics;

/*
 * Animation & Timing 101
 * ----------------------
 * `Animation` is a tiny state machine that handles brightness fades. Call
 * `triggerFade()` to kick it awake and `updateFadeAnimation()` each loop to
 * march through `FADE_IN`, `HOLD`, and `FADE_OUT` states based on `lastTime`
 * and `duration`. Once the party hits `DONE`, the screen stays put until the
 * next trigger.
 *
 * `updateFromContext()` is the high‑level refresh that keeps the UI snappy
 * without frying the MCU. It bails out early unless at least `_updateIntervalMs`
 * milliseconds have passed since the last update. Tweak that interval with
 * `setUpdateInterval()` to get your preferred balance of responsiveness and
 * chill.
 *
 * Example mash‑up: fade into the screensaver when the user ghosts you:
 *
 * ```cpp
 * DisplayManager ui(0x3C, OLED_WIDTH, OLED_HEIGHT);
 * // inside loop()
 * if (ui.shouldRunScreensaver()) {
 *   ui.triggerFade(500);          // half‑second fade out
 *   while (ui.shouldRunScreensaver()) {
 *     ui.updateFadeAnimation();
 *     ui.runIdleScreensaver();   // random pixel mayhem
 *   }
 * }
 * ```
 */

enum class AnimState { IDLE, FADE_IN, HOLD, FADE_OUT, DONE };

/**
 * Simple helper struct used for fade animations. It's fed by `triggerFade()`
 * and advanced by `updateFadeAnimation()`.
 */
struct Animation {
    AnimState state = AnimState::IDLE; //!< Current animation state
    uint32_t lastTime = 0;             //!< Timestamp of last update
    uint16_t duration = 0;             //!< Duration of the fade portion
    uint8_t brightness = 0;            //!< Current display brightness
};

// Startup splash, non-blocking because we're not monsters.
enum class StartupPhase { IDLE, DRAW_LINES, HOLD_LINES, HOLD_LOGO, DONE };

/**
 * Tracks progress of the boot animation so `runStartupAnimation()` can be
 * called every loop without resorting to `delay()`. Think of it as a very tiny
 * punk rock state machine.
 */
struct StartupAnimation {
    StartupPhase phase = StartupPhase::IDLE; //!< Current animation phase
    uint8_t step = 0;                        //!< Line pattern step
    uint32_t lastTime = 0;                   //!< Time of last phase advance
};

/**
 * @brief Wrapper around the Adafruit SSD1306 library providing
 *        higher level UI helpers.
 */
class DisplayManager {
  public:
    // Diagnostic page indices are shared with ButtonManager so long-press navigation
    // never drifts out of sync with what the OLED actually renders.
    static constexpr uint8_t kDiagnosticPageCount = 5;
    static constexpr uint8_t kDiagnosticPageDebug = 4;

    /**
     * @param i2cAddress   I2C address of the SSD1306 display.
     * @param screenWidth  Display width in pixels.
     * @param screenHeight Display height in pixels.
     */
    DisplayManager(uint8_t i2cAddress, uint16_t screenWidth, uint16_t screenHeight);

    /** Initialise the underlying display hardware. Call from setup(). */
    bool begin();

    // Core display methods ------------------------------------------------

    /** Clear the screen and show up to three lines of text. */
    void showText(const char *line1, const char *line2 = "", const char *line3 = "");

    /** Convenience helper for displaying a single numeric value. */
    void showValue(uint8_t value, bool clearDisplay = true);

    /** Show which envelope follower is assigned to a slot. */
    void showEnvelopeAssignment(int potIndex, int efIndex, const char *mode, const char *argMethod);

    /** Display the current operating mode string on the bottom row. */
    void showMode(const char *mode, bool clearDisplay = true);

    /** Clear the display buffer immediately. */
    void clear();

    /** Update the main UI view with real‑time information. */
    void updateDisplay(uint8_t beatPosition, const uint8_t *envelopeLevels, size_t envelopeCount,
                       const char *statusMessage, uint8_t activePot, uint8_t activeChannel,
                       const char *envelopeMode);

    /** Display a transient message for the given duration. */
    void displayStatus(const char *status, unsigned long duration);

    /**
     * Refresh the UI using values from a ButtonManagerContext.
     * Call this from your main loop; it quietly returns if invoked
     * before `_updateIntervalMs` has elapsed since the last refresh.
     */
    void updateFromContext(const ButtonManagerContext &context);

    /** Show the selected ARG method and envelope pairing. */
    void showARGInfo(const char *methodName, int envA, int envB);

    /** Set a message that overrides the UI for a short time. */
    void setTemporaryMessage(const char *message, unsigned long duration);

    /** Display a human readable representation of a MIDI message. */
    void showMIDIMessage(uint8_t cc, uint8_t value, uint8_t channel);

    /** Show diagnostics pages when the box is in self-test mode. */
    void showDiagnostic(uint8_t page, const ButtonManager &bm, const ButtonManagerContext &ctx,
                        const MIDIHandler &midi, const SystemDiagnostics &diag);

    /** Update the beat indicator or show "--" when clock is stopped. */
    void updateBeat(uint8_t beatPosition, bool clockRunning);

    // Advanced features ---------------------------------------------------

    /** Begin a manual drawing session by clearing the buffer. */
    void beginDraw();

    /** End a manual drawing session and push pixels to the display. */
    void endDraw();

    /** Display an error message; optionally leave it on screen indefinitely. */
    void showError(const char *errorMessage, bool persistent = false);

    /** Draw a single vertical envelope meter. */
    void showEnvelopeLevel(uint8_t level);

    /** Draw two envelope level bars used in ARG mode. */
    void showEnvelopeLevels(uint8_t envA, uint8_t envB);

    /** Cache the active pot and channel for later updates. */
    void updateActiveSelection(uint8_t activePot, uint8_t activeChannel);

    /** Highlight a particular pot in custom UI screens. */
    void highlightActivePot(uint8_t potIndex);

    /** Highlight the active mode string along the bottom of the display. */
    void highlightActiveMode(const String &modeName);

    /** Configure how often updateFromContext refreshes the display. */
    void setUpdateInterval(unsigned long intervalMs);

    /** Return the current update interval in milliseconds. */
    unsigned long getUpdateInterval() const;

    /** Trigger a short fade in/out animation. */
    void triggerFade(uint16_t ms);

    /** Update the fade animation state machine. */
    void updateFadeAnimation();

    /** Simple splash screen shown at startup. */
    void runStartupAnimation();

    /** Random pixel screensaver used after long inactivity. */
    void runIdleScreensaver();

    /** Record user interaction to postpone screensaver. */
    void registerInteraction();

    /** Check if the screensaver should currently run. */
    bool shouldRunScreensaver() const;

    /** Display helper for tuning filter frequency and resonance. */
    void showFilterTuning(const char *labelFreq, float freqValue, const char *labelQ, float qValue);

    /** Display helper for arpeggiator length (in MIDI clock ticks) and shape. */
    void showArpSettings(uint8_t lengthTicks, const char *shapeName);

  private:
    Animation _fadeAnim;
    Adafruit_SSD1306 _display;
    uint8_t _i2cAddress;

    String _statusMessage;
    unsigned long _statusTimeout;

    bool _isDrawing;
    unsigned long _updateIntervalMs;
    unsigned long _lastDisplayPushMs = 0;
    unsigned long _lastFullRefreshMs = 0;
    std::size_t _frameBufferBytes = 0;
    bool _shadowValid = false;
    static constexpr std::size_t kMaxFrameBufferBytes =
        static_cast<std::size_t>(OLED_WIDTH) * ((static_cast<std::size_t>(OLED_HEIGHT) + 7U) / 8U);
    std::array<uint8_t, kMaxFrameBufferBytes> _lastPushedFrame{};
    unsigned long _lastInteractionTime;
    uint8_t _activePot;
    uint8_t _activeChannel;
    String _activeMode;

    StartupAnimation _startupAnim; //!< State tracker for the boot splash

    void drawBorder();
    void present(bool force = false);
    void syncShadowBuffer();
};

#endif // DISPLAYMANAGER_H
