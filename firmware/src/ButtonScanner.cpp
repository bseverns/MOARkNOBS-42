#include "ButtonScanner.h"

#include "Globals.h"
#include "Hardware/IO.h"
#include "Utility.h"

ButtonScanner::ButtonScanner(const HardwareConfig &config, const uint8_t *controlPins)
    : _config(config), _controlPins(controlPins) {
    resetState();
}

void ButtonScanner::resetState() {
    for (uint8_t i = 0; i < kTotalButtons; ++i) {
        _stableStates[i] = false;
        _lastRawStates[i] = false;
        _lastDebounceTimes[i] = 0;
    }
    for (uint8_t i = 0; i < 3; ++i) {
        _controlPotRaw[i] = 0;
    }
    _currentRow = 0;
    _cachedRow = 0xFF;
}

void ButtonScanner::initHardware() {
    for (uint8_t i = 0; i < kMuxSelectPins; ++i) {
        pinMode(_config.muxrPins[i], OUTPUT);
        pinMode(_config.muxcPins[i], OUTPUT);
    }
    pinMode(_config.buttonMuxAnalogPin, INPUT);
    pinMode(_config.rowDriverPin, OUTPUT);
    digitalWrite(_config.rowDriverPin, LOW);
    resetState();
}

void ButtonScanner::waitForMuxSettle() {
    const uint32_t start = micros();
    while (micros() - start < kMuxSettleUs) {
        yield();
    }
}

void ButtonScanner::setMuxFast(const uint8_t selectPins[4], uint8_t index) {
    static const uint8_t lut[16][4] = {{0, 0, 0, 0}, {1, 0, 0, 0}, {0, 1, 0, 0}, {1, 1, 0, 0},
                                       {0, 0, 1, 0}, {1, 0, 1, 0}, {0, 1, 1, 0}, {1, 1, 1, 0},
                                       {0, 0, 0, 1}, {1, 0, 0, 1}, {0, 1, 0, 1}, {1, 1, 0, 1},
                                       {0, 0, 1, 1}, {1, 0, 1, 1}, {0, 1, 1, 1}, {1, 1, 1, 1}};
    const uint8_t *bits = lut[index & 0x0F];
    digitalWriteFast(selectPins[0], bits[0]);
    digitalWriteFast(selectPins[1], bits[1]);
    digitalWriteFast(selectPins[2], bits[2]);
    digitalWriteFast(selectPins[3], bits[3]);
}

void ButtonScanner::selectMux(uint8_t row, uint8_t col) const {
    setMuxFast(_config.muxrPins, row);
    setMuxFast(_config.muxcPins, col);
}

MatrixScanRange ButtonScanner::scanNextMatrixRow(unsigned long now) {
    const uint8_t row = _currentRow;
    const uint8_t begin = static_cast<uint8_t>(row * BUTTON_COLS);
    const uint8_t end = static_cast<uint8_t>(begin + BUTTON_COLS);

    digitalWrite(_config.rowDriverPin, HIGH);
    setMuxFast(_config.muxrPins, row);
    waitForMuxSettle();
    for (uint8_t col = 0; col < BUTTON_COLS; ++col) {
        setMuxFast(_config.muxcPins, col);
        waitForMuxSettle();
        const uint8_t index = static_cast<uint8_t>(begin + col);
        const bool raw = hardware::readAnalog(_config.buttonMuxAnalogPin) < BUTTON_PRESS_THRESHOLD;
        _cachedRowValues[col] = raw ? HIGH : LOW;
        Utility::debounce(_stableStates[index], _lastRawStates[index], raw,
                          _lastDebounceTimes[index], now, DEBOUNCE_DELAY);
    }
    digitalWrite(_config.rowDriverPin, LOW);

    _cachedRow = row;
    _currentRow = static_cast<uint8_t>((row + 1) % BUTTON_ROWS);
    return {begin, end};
}

void ButtonScanner::scanControlBank(unsigned long now) {
    for (uint8_t channel = 6; channel < 12; ++channel) {
        selectMux(0, channel);
        waitForMuxSettle();
        const uint8_t controlIndex = static_cast<uint8_t>(channel - 6);
        const uint8_t index = static_cast<uint8_t>(NUM_VIRTUAL_BUTTONS + controlIndex);
        const bool raw = hardware::readAnalog(_config.buttonMuxAnalogPin) < BUTTON_PRESS_THRESHOLD;
        Utility::debounce(_stableStates[index], _lastRawStates[index], raw,
                          _lastDebounceTimes[index], now, DEBOUNCE_DELAY);
    }

    for (uint8_t index = 0; index < 3; ++index) {
        selectMux(0, static_cast<uint8_t>(12 + index));
        waitForMuxSettle();
        _controlPotRaw[index] = hardware::readAnalog(_config.buttonMuxAnalogPin);
    }
}

bool ButtonScanner::isPressed(uint8_t index) const {
    return index < kTotalButtons && _stableStates[index];
}

uint8_t ButtonScanner::controlMask() const {
    uint8_t mask = 0;
    for (uint8_t index = 0; index < NUM_CONTROL_BUTTONS; ++index) {
        if (isPressed(static_cast<uint8_t>(NUM_VIRTUAL_BUTTONS + index))) {
            mask |= static_cast<uint8_t>(1U << index);
        }
    }
    return mask;
}

int ButtonScanner::controlPotRaw(uint8_t index) const {
    return index < 3 ? _controlPotRaw[index] : 0;
}

uint8_t ButtonScanner::readMatrixButton(uint8_t index) const {
    if (index >= NUM_VIRTUAL_BUTTONS) return LOW;
    const uint8_t row = static_cast<uint8_t>(index / BUTTON_COLS);
    const uint8_t col = static_cast<uint8_t>(index % BUTTON_COLS);
    if (row != _cachedRow) {
        digitalWrite(_config.rowDriverPin, HIGH);
        setMuxFast(_config.muxrPins, row);
        waitForMuxSettle();
        for (uint8_t candidate = 0; candidate < BUTTON_COLS; ++candidate) {
            setMuxFast(_config.muxcPins, candidate);
            waitForMuxSettle();
            _cachedRowValues[candidate] =
                hardware::readAnalog(_config.buttonMuxAnalogPin) < BUTTON_PRESS_THRESHOLD ? HIGH
                                                                                          : LOW;
        }
        digitalWrite(_config.rowDriverPin, LOW);
        _cachedRow = row;
    }
    return _cachedRowValues[col];
}

bool ButtonScanner::readDirectControlButton(uint8_t index) const {
    return index < NUM_CONTROL_BUTTONS && _controlPins != nullptr &&
           hardware::readDigital(_controlPins[index]) == LOW;
}
