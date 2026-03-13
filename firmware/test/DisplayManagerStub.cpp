#include "DisplayManager.h"

#include "ButtonManager.h"
#include "TimeUtils.h"

#include <climits>

DisplayManager::DisplayManager(uint8_t i2cAddress, uint16_t screenWidth, uint16_t screenHeight)
    : _display(screenWidth, screenHeight, &Wire), _i2cAddress(i2cAddress) {
    _statusTimeout = 0;
    _isDrawing = false;
    _updateIntervalMs = 100;
    _lastInteractionTime = now();
    _activePot = 0;
    _activeChannel = 1;
    _activeMode = "MIDI";
}

bool DisplayManager::begin() { return true; }

void DisplayManager::showText(const char *line1, const char *line2, const char *line3) {
    _statusMessage = line1 ? line1 : "";
    if (line2 && line2[0] != '\0') {
        _statusMessage += "\n";
        _statusMessage += line2;
    }
    if (line3 && line3[0] != '\0') {
        _statusMessage += "\n";
        _statusMessage += line3;
    }
}

void DisplayManager::showValue(uint8_t, bool) {}
void DisplayManager::showEnvelopeAssignment(int, int, const char *, const char *) {}
void DisplayManager::showMode(const char *, bool) {}
void DisplayManager::clear() { _statusMessage = ""; }

void DisplayManager::updateDisplay(uint8_t, const uint8_t *, size_t, const char *statusMessage,
                                   uint8_t activePot, uint8_t activeChannel,
                                   const char *envelopeMode) {
    _activePot = activePot;
    _activeChannel = activeChannel;
    _activeMode = envelopeMode ? envelopeMode : "";
    if (statusMessage) {
        _statusMessage = statusMessage;
    }
}

void DisplayManager::displayStatus(const char *status, unsigned long duration) {
    setTemporaryMessage(status, duration);
}

void DisplayManager::updateFromContext(const ButtonManagerContext &context) {
    _activePot = context.activePot;
    _activeChannel = context.activeChannel;
    _activeMode = context.envelopeMode ? context.envelopeMode : "";
}

void DisplayManager::showARGInfo(const char *, int, int) {}

void DisplayManager::setTemporaryMessage(const char *message, unsigned long duration) {
    _statusMessage = message ? message : "";
    _statusTimeout = now() + duration;
}

void DisplayManager::showMIDIMessage(uint8_t, uint8_t, uint8_t) {}
void DisplayManager::showDiagnostic(uint8_t, const ButtonManager &, const ButtonManagerContext &,
                                    const MIDIHandler &, const SystemDiagnostics &) {}
void DisplayManager::updateBeat(uint8_t, bool) {}

void DisplayManager::beginDraw() { _isDrawing = true; }
void DisplayManager::endDraw() { _isDrawing = false; }

void DisplayManager::showError(const char *errorMessage, bool persistent) {
    setTemporaryMessage(errorMessage, persistent ? ULONG_MAX : 1000UL);
}

void DisplayManager::showEnvelopeLevel(uint8_t) {}
void DisplayManager::showEnvelopeLevels(uint8_t, uint8_t) {}

void DisplayManager::updateActiveSelection(uint8_t activePot, uint8_t activeChannel) {
    _activePot = activePot;
    _activeChannel = activeChannel;
}

void DisplayManager::highlightActivePot(uint8_t potIndex) { _activePot = potIndex; }

void DisplayManager::highlightActiveMode(const String &modeName) { _activeMode = modeName; }

void DisplayManager::setUpdateInterval(unsigned long intervalMs) { _updateIntervalMs = intervalMs; }

unsigned long DisplayManager::getUpdateInterval() const { return _updateIntervalMs; }

void DisplayManager::triggerFade(uint16_t ms) {
    _fadeAnim.duration = ms;
    _fadeAnim.lastTime = now();
}

void DisplayManager::updateFadeAnimation() {}

void DisplayManager::runStartupAnimation() { _startupAnim.phase = StartupPhase::DONE; }

void DisplayManager::runIdleScreensaver() {}

void DisplayManager::registerInteraction() { _lastInteractionTime = now(); }

bool DisplayManager::shouldRunScreensaver() const {
    return (now() - _lastInteractionTime) > 90000UL;
}

void DisplayManager::showFilterTuning(const char *, float, const char *, float) {}

void DisplayManager::showArpSettings(uint8_t, const char *) {}
