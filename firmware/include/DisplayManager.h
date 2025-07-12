#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>
#include "Globals.h"    // for SCREEN_WIDTH, SCREEN_HEIGHT

struct ButtonManagerContext;

enum class AnimState { IDLE, FADE_IN, HOLD, FADE_OUT, DONE };

/** Simple helper struct used for fade animations. */
struct Animation {
  AnimState  state      = AnimState::IDLE; //!< Current animation state
  uint32_t   lastTime   = 0;               //!< Timestamp of last update
  uint16_t   duration   = 0;               //!< Duration of the fade portion
  uint8_t    brightness = 0;               //!< Current display brightness
};

/**
 * @brief Wrapper around the Adafruit SSD1306 library providing
 *        higher level UI helpers.
 */
class DisplayManager {
public:
  /**
   * @param i2cAddress   I2C address of the SSD1306 display.
   * @param screenWidth  Display width in pixels.
   * @param screenHeight Display height in pixels.
   */
  DisplayManager(uint8_t i2cAddress, uint16_t screenWidth, uint16_t screenHeight);

  /** Initialise the underlying display hardware. */
  bool begin();

  // Core display methods ------------------------------------------------

  /** Display up to three lines of text. */
  void showText(const char* line1, const char* line2 = "", const char* line3 = "");

  /** Convenience for showing a numeric value. */
  void showValue(uint8_t value, bool clearDisplay = true);

  /** Show which envelope follower is assigned to a slot. */
  void showEnvelopeAssignment(int potIndex, int efIndex, const char* mode, const char* argMethod);

  /** Display the current operating mode string. */
  void showMode(const char* mode, bool clearDisplay = true);

  /** Clear the display immediately. */
  void clear();

  /** Update the main UI view with real‑time information. */
  void updateDisplay(uint8_t beatPosition,
                     const std::vector<uint8_t>& envelopeLevels,
                     const char* statusMessage,
                     uint8_t activePot,
                     uint8_t activeChannel,
                     const char* envelopeMode);

  /** Show a short status message for a fixed duration. */
  void displayStatus(const char* status, unsigned long duration);

  /** Render state based on a ButtonManagerContext snapshot. */
  void updateFromContext(const ButtonManagerContext& context);

  /** Display info about the current ARG method and envelope pair. */
  void showARGInfo(const char* methodName, int envA, int envB);

  /** Display a temporary message without disturbing the main UI. */
  void setTemporaryMessage(const char* message, unsigned long duration);

  /** Display a human readable representation of a MIDI message. */
  void showMIDIMessage(uint8_t cc, uint8_t value, uint8_t channel);

  /** Display the current beat or 'no clock' state. */
  void updateBeat(uint8_t beatPosition, bool clockRunning);

  // Advanced features ---------------------------------------------------

  /** Begin a manual drawing session (clears the screen). */
  void beginDraw();

  /** End a manual drawing session and push pixels to the display. */
  void endDraw();

  /** Show an error message, optionally halting the program. */
  void showError(const char* errorMessage, bool persistent = false);

  /** Draw a single envelope level bar. */
  void showEnvelopeLevel(uint8_t level);

  /** Draw two envelope level bars used in ARG mode. */
  void showEnvelopeLevels(uint8_t envA, uint8_t envB);

  /** Update cached selection indices. */
  void updateActiveSelection(uint8_t activePot, uint8_t activeChannel);

  /** Highlight a particular pot in custom UI screens. */
  void highlightActivePot(uint8_t potIndex);

  /** Highlight the active mode string along the bottom of the display. */
  void highlightActiveMode(const String& modeName);

  /** Set how often updateFromContext should refresh the display. */
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
  void showFilterTuning(const char* labelFreq, float freqValue, const char* labelQ, float qValue);

private:
  Animation          _fadeAnim;
  Adafruit_SSD1306   _display;
  uint8_t            _i2cAddress;

  String             _statusMessage;
  unsigned long      _statusTimeout;

  bool               _isDrawing;
  unsigned long      _updateIntervalMs;
  unsigned long      _lastInteractionTime;
  uint8_t            _activePot;
  uint8_t            _activeChannel;
  String             _activeMode;

  void drawBorder();
};

#endif // DISPLAYMANAGER_H
