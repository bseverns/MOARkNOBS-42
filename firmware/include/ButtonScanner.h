#ifndef BUTTON_SCANNER_H
#define BUTTON_SCANNER_H

#include <Arduino.h>

#include "ButtonInputConstants.h"

struct HardwareConfig;

struct MatrixScanRange {
    uint8_t begin;
    uint8_t end;
};

// Owns the electrical button boundary: mux selection, settling, raw ADC/GPIO
// reads, debounce state, and the latest stable physical state. It deliberately
// does not interpret presses as gestures or mutate instrument configuration.
class ButtonScanner {
  public:
    ButtonScanner(const HardwareConfig &config, const uint8_t *controlPins);

    void initHardware();
    MatrixScanRange scanNextMatrixRow(unsigned long now);
    void scanControlBank(unsigned long now);

    bool isPressed(uint8_t index) const;
    uint8_t controlMask() const;
    int controlPotRaw(uint8_t index) const;

    uint8_t readMatrixButton(uint8_t index) const;
    bool readDirectControlButton(uint8_t index) const;

  private:
    static constexpr uint8_t kMuxSelectPins = 4;
    static constexpr uint32_t kMuxSettleUs = 5;
    static constexpr uint8_t kTotalButtons = NUM_BUTTON_INPUTS;

    const HardwareConfig &_config;
    const uint8_t *_controlPins;
    bool _stableStates[kTotalButtons] = {false};
    bool _lastRawStates[kTotalButtons] = {false};
    unsigned long _lastDebounceTimes[kTotalButtons] = {0};
    int _controlPotRaw[3] = {0};
    uint8_t _currentRow = 0;
    mutable uint8_t _cachedRow = 0xFF;
    mutable uint8_t _cachedRowValues[BUTTON_COLS] = {LOW};

    void resetState();
    void selectMux(uint8_t row, uint8_t col) const;
    static void setMuxFast(const uint8_t selectPins[4], uint8_t index);
    static void waitForMuxSettle();
};

#endif // BUTTON_SCANNER_H
