#include "unity_config.h"
#include <unity.h>

#include <vector>

#include "ButtonManager.h"
#include "ButtonScanner.h"
#include "Hardware/IO.h"
#include "Globals.h"

namespace {

// This file is our sandbox for the hardware::IO indirection layer.  By hot-
// swapping providers we emulate analog/digital readings without touching real
// silicon and prove the shims behave like the live firmware.

int stubAnalogA(uint8_t) { return 111; }
int stubAnalogB(uint8_t) { return 222; }

int stubDigitalHigh(uint8_t) { return HIGH; }
int stubDigitalLow(uint8_t) { return LOW; }

struct AnalogSequence {
    const int *values;
    size_t size;
    size_t index;
};

AnalogSequence sequence{nullptr, 0, 0};

int sequenceAnalog(uint8_t) {
    if (!sequence.values || sequence.size == 0) {
        return 0;
    }
    int value = sequence.values[sequence.index % sequence.size];
    sequence.index++;
    return value;
}

class ScopedSequence {
  public:
    explicit ScopedSequence(const std::initializer_list<int> &vals) : guard_(sequenceAnalog) {
        buffer_.assign(vals.begin(), vals.end());
        sequence.values = buffer_.data();
        sequence.size = buffer_.size();
        sequence.index = 0;
    }

    ~ScopedSequence() {
        sequence.values = nullptr;
        sequence.size = 0;
        sequence.index = 0;
    }

  private:
    std::vector<int> buffer_;
    hardware::ScopedAnalogReadProvider guard_;
};

} // namespace

// Nesting ScopedAnalogReadProvider instances should restore the previous
// callback once the inner guard dies.  This verifies the stack discipline.
void test_scoped_analog_provider_nesting() {
    hardware::ScopedAnalogReadProvider outer(stubAnalogA);
    TEST_ASSERT_EQUAL(111, hardware::readAnalog(0));
    {
        hardware::ScopedAnalogReadProvider inner(stubAnalogB);
        TEST_ASSERT_EQUAL(222, hardware::readAnalog(0));
    }
    TEST_ASSERT_EQUAL(111, hardware::readAnalog(0));
}

// Swap in a deterministic analog sequence and make sure the mux reader cycles
// through values like a firmware scan would.
void test_sequence_provider_cycles_values() {
    ScopedSequence values{0, 512, 1023};
    TEST_ASSERT_EQUAL(0, hardware::readAnalog(0));
    TEST_ASSERT_EQUAL(512, hardware::readAnalog(0));
    TEST_ASSERT_EQUAL(1023, hardware::readAnalog(0));
    TEST_ASSERT_EQUAL(0, hardware::readAnalog(0));
}

// setAnalogReadProvider() should hand you the previous callback so callers can
// unwind gracefully.  This covers that handshake.
void test_set_provider_returns_previous() {
    auto baseline = hardware::currentAnalogReadProvider();
    auto previous = hardware::setAnalogReadProvider(stubAnalogA);
    TEST_ASSERT_EQUAL_PTR(baseline, previous);
    TEST_ASSERT_EQUAL(111, hardware::readAnalog(0));
    auto swapped = hardware::setAnalogReadProvider(stubAnalogB);
    TEST_ASSERT_EQUAL_PTR(stubAnalogA, swapped);
    TEST_ASSERT_EQUAL(222, hardware::readAnalog(0));
    hardware::setAnalogReadProvider(baseline);
}

void test_digital_provider_overrides_matrix_reads() {
    ButtonScanner scanner(hwConfig);
    hardware::ScopedDigitalReadProvider pressed(stubDigitalHigh);
    scanner.scanControlBank(0);
    scanner.scanControlBank(DEBOUNCE_DELAY + 1);
    TEST_ASSERT_EQUAL_HEX8(0x3F, scanner.controlMask());
}

void test_button_scanner_debounces_one_matrix_row_per_pass() {
    ButtonScanner scanner(hwConfig);
    hardware::ScopedDigitalReadProvider pressed(stubDigitalLow);

    for (uint8_t row = 0; row < BUTTON_ROWS; ++row) {
        const MatrixScanRange range = scanner.scanNextMatrixRow(0);
        TEST_ASSERT_EQUAL_UINT8(row * BUTTON_COLS, range.begin);
        TEST_ASSERT_EQUAL_UINT8((row + 1) * BUTTON_COLS, range.end);
    }
    TEST_ASSERT_FALSE(scanner.isPressed(0));

    for (uint8_t row = 0; row < BUTTON_ROWS; ++row) {
        scanner.scanNextMatrixRow(DEBOUNCE_DELAY + 1);
    }
    TEST_ASSERT_TRUE(scanner.isPressed(0));
    TEST_ASSERT_TRUE(scanner.isPressed(NUM_VIRTUAL_BUTTONS - 1));
}

void test_button_scanner_reports_debounced_control_mask_and_raw_pots() {
    ButtonScanner scanner(hwConfig);
    hardware::ScopedDigitalReadProvider pressed(stubDigitalHigh);

    scanner.scanControlBank(0);
    TEST_ASSERT_EQUAL_HEX8(0, scanner.controlMask());
    scanner.scanControlBank(DEBOUNCE_DELAY + 1);

    TEST_ASSERT_EQUAL_HEX8(0x3F, scanner.controlMask());
    TEST_ASSERT_FALSE(scanner.controlPotsAvailable());
}

// Boot-time tuning must not rewrite topology already captured by global
// managers. Keep this contract explicit as new hardware fields are added.
void test_bench_pin_contract_has_no_i2c_or_vref_conflicts() {
    TEST_ASSERT_EQUAL_UINT8(22, hwConfig.rowDriverPin);
    TEST_ASSERT_EQUAL_UINT8(7, hwConfig.matrixInputPin);
    TEST_ASSERT_EQUAL_UINT8(kUnavailablePin, hwConfig.slotPotMuxAnalogPin);
    TEST_ASSERT_EQUAL_UINT8(kUnavailablePin, hwConfig.vrefAdcPin);
    TEST_ASSERT_FALSE(hwConfig.controlPotsAvailable);
}

void test_runtime_hardware_tuning_preserves_structural_config() {
    HardwareConfig config = hwConfig;
    const HardwareConfig original = config;
    const HardwareRuntimeTuning tuning = {7, 11, 13, 17};

    applyHardwareRuntimeTuning(config, tuning);

    TEST_ASSERT_EQUAL_UINT8(original.ledPin, config.ledPin);
    TEST_ASSERT_EQUAL_UINT8(original.statusLedPin, config.statusLedPin);
    TEST_ASSERT_EQUAL_UINT8(original.rowDriverPin, config.rowDriverPin);
    TEST_ASSERT_EQUAL_UINT16(original.slotLedCount, config.slotLedCount);
    TEST_ASSERT_EQUAL_UINT8(original.efLedCount, config.efLedCount);
    TEST_ASSERT_EQUAL_UINT8(original.potLedCount, config.potLedCount);
    TEST_ASSERT_EQUAL_UINT8(original.numButtons, config.numButtons);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(original.muxrPins, config.muxrPins, 4);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(original.muxcPins, config.muxcPins, 4);
    TEST_ASSERT_EQUAL_UINT8(original.matrixInputPin, config.matrixInputPin);
    TEST_ASSERT_EQUAL_UINT8(original.slotPotMuxAnalogPin, config.slotPotMuxAnalogPin);
    TEST_ASSERT_EQUAL_UINT8(original.vrefAdcPin, config.vrefAdcPin);
    TEST_ASSERT_EQUAL(original.controlPotsAvailable, config.controlPotsAvailable);
    TEST_ASSERT_EQUAL_UINT8(7, config.midiTaskInterval);
    TEST_ASSERT_EQUAL_UINT8(11, config.serialTaskInterval);
    TEST_ASSERT_EQUAL_UINT8(13, config.ledTaskInterval);
    TEST_ASSERT_EQUAL_UINT8(17, config.envelopeTaskInterval);
}

void test_runtime_hardware_tuning_rejects_zero_intervals() {
    HardwareConfig config = hwConfig;
    applyHardwareRuntimeTuning(config, {0, 0, 0, 0});

    TEST_ASSERT_EQUAL_UINT8(1, config.midiTaskInterval);
    TEST_ASSERT_EQUAL_UINT8(1, config.serialTaskInterval);
    TEST_ASSERT_EQUAL_UINT8(1, config.ledTaskInterval);
    TEST_ASSERT_EQUAL_UINT8(1, config.envelopeTaskInterval);
}

#if defined(UNIT_TEST) && !defined(UNIT_TEST_PROTOCOL_IMPL)

ButtonManager::ButtonManager(const HardwareConfig &config, PotentiometerManager *potentiometerManager)
    : _scanner(config), _potentiometerManager(potentiometerManager), activeMode(0),
      _pendingEfSlot(-1), _efAssignDeadline(0) {}

#endif // defined(UNIT_TEST)
