#include "unity_config.h"
#include <unity.h>

#include "Utility.h"
#include "ConfigManager.h"
#include <EEPROM.h>

void test_process_bulk_update_writes_expected_layout() {
    constexpr uint8_t numPotsUnderTest = 4;
    constexpr uint8_t sentinel = 0xAA;

    const uint16_t layoutGuardEnd = EEPROM_POT_CC + numPotsUnderTest + 1;
    for (uint16_t address = 0; address <= layoutGuardEnd; ++address) {
        EEPROM.update(address, sentinel);
    }

    struct Mapping {
        uint8_t cc;
        uint8_t channel;
    };

    const Mapping assignments[numPotsUnderTest] = {
        {12, 2},
        {34, 7},
        {56, 13},
        {78, 16},
    };

    String command = "SET_ALL ";
    for (uint8_t i = 0; i < numPotsUnderTest; ++i) {
        if (i > 0) {
            command += ';';
        }
        command += String(assignments[i].cc);
        command += ',';
        command += String(assignments[i].channel);
    }
    command += ";99,5"; // extra payload should be ignored once we run out of pots

    Utility::processBulkUpdate(command, numPotsUnderTest);

    for (uint8_t i = 0; i < numPotsUnderTest; ++i) {
        TEST_ASSERT_EQUAL_UINT8(assignments[i].channel,
                                EEPROM.read(EEPROM_POT_CHANNELS + i));
        TEST_ASSERT_EQUAL_UINT8(assignments[i].cc, EEPROM.read(EEPROM_POT_CC + i));
        TEST_ASSERT_EQUAL_UINT8(sentinel, EEPROM.read(2 * i + 1));
    }

    TEST_ASSERT_EQUAL_UINT8(sentinel, EEPROM.read(2 * numPotsUnderTest));
    TEST_ASSERT_EQUAL_UINT8(sentinel, EEPROM.read(EEPROM_POT_CHANNELS + numPotsUnderTest));
    TEST_ASSERT_EQUAL_UINT8(sentinel, EEPROM.read(EEPROM_POT_CC + numPotsUnderTest));
}
