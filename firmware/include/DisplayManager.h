#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>
#include "Globals.h"    // for SCREEN_WIDTH, SCREEN_HEIGHT

struct ButtonManagerContext;

enum class AnimState { IDLE, FADE_IN, HOLD, FADE_OUT, DONE };

struct Animation {
  AnimState  state      = AnimState::IDLE;
  uint32_t   lastTime   = 0;
  uint16_t   duration   = 0;
  uint8_t    brightness = 0;
};

class DisplayManager {
public:
  DisplayManager(uint8_t i2cAddress, uint16_t screenWidth, uint16_t screenHeight);
  bool begin();

  // Core display methods
  void showText(const char* line1, const char* line2 = "", const char* line3 = "");
  void showValue(uint8_t value, bool clearDisplay = true);
  void showEnvelopeAssignment(int potIndex, int efIndex, const char* mode, const char* argMethod);
  void showMode(const char* mode, bool clearDisplay = true);
  void clear();
  void updateDisplay(uint8_t beatPosition,
                     const std::vector<uint8_t>& envelopeLevels,
                     const char* statusMessage,
                     uint8_t activePot,
                     uint8_t activeChannel,
                     const char* envelopeMode);
  void displayStatus(const char* status, unsigned long duration);
  void updateFromContext(const ButtonManagerContext& context);
  void showARGInfo(const char* methodName, int envA, int envB);
  void setTemporaryMessage(const char* message, unsigned long duration);
  void showMIDIMessage(uint8_t cc, uint8_t value, uint8_t channel);
  void updateBeat(uint8_t beatPosition, bool clockRunning);

  // Advanced features
  void beginDraw();
  void endDraw();
  void showError(const char* errorMessage, bool persistent = false);
  void showEnvelopeLevel(uint8_t level);
  void showEnvelopeLevels(uint8_t envA, uint8_t envB);
  void updateActiveSelection(uint8_t activePot, uint8_t activeChannel);
  void highlightActivePot(uint8_t potIndex);
  void highlightActiveMode(const String& modeName);

  void setUpdateInterval(unsigned long intervalMs);
  unsigned long getUpdateInterval() const;

  void triggerFade(uint16_t ms);
  void updateFadeAnimation();
  void runStartupAnimation();
  void runIdleScreensaver();
  void registerInteraction();
  bool shouldRunScreensaver() const;
  void showFilterTuning(float frequency, float q);

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
