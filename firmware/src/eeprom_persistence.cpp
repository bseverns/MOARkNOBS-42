#include <Arduino.h>
#include "Globals.h"
#include "Utility.h"
#include "ConfigManager.h"
#include "LEDManager.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"
#include "EnvelopeFollower.h"

// -- Constants ---------------------------------------------------------------
// address well outside normal config space
#define EEPROM_TEST_FLAG_ADDR (EEPROM_BACKUP_START + 100)
#define TEST_NOT_STARTED 0xFF
#define TEST_STAGE_SAVE  0x55
#define TEST_STAGE_VERIFY 0xAA

// -- Globals -----------------------------------------------------------------
std::vector<uint8_t> potChannels;
ConfigManager       configManager(NUM_POTS, NUM_BUTTONS);
LEDManager          ledManager(NUM_LEDS);
DisplayManager      displayManager(SSD1306_I2C_ADDRESS, OLED_WIDTH, OLED_HEIGHT);
PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
ButtonManager       buttonManager(primaryMuxPins, secondaryMuxPins, buttonMuxAnalogPin,
                                  (const uint8_t[]){12,13,14,15,24,25},
                                  &potentiometerManager);
std::vector<EnvelopeFollower> envelopeFollowers = {
    EnvelopeFollower(A0, &potentiometerManager),
    EnvelopeFollower(A1, &potentiometerManager),
    EnvelopeFollower(A2, &potentiometerManager),
    EnvelopeFollower(A3, &potentiometerManager),
    EnvelopeFollower(A6, &potentiometerManager),
    EnvelopeFollower(A7, &potentiometerManager),
};

static MIDISlot testSlots[NUM_SLOTS];

// -----------------------------------------------------------------------------
void fillTestData() {
    for (uint8_t i = 0; i < NUM_POTS; ++i) {
        configManager.setPotChannel(i, (i % 16) + 1);
        configManager.setPotCCNumber(i, 10 + i);
    }
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        testSlots[i] = {MIDIMessageType::CC, (uint8_t)((i % 16) + 1), i, (uint8_t)(i % 6), true};
    }
}

bool verifyTestData() {
    for (uint8_t i = 0; i < NUM_POTS; ++i) {
        if (configManager.getPotChannel(i) != (i % 16) + 1) return false;
        if (configManager.getPotCCNumber(i) != 10 + i)     return false;
    }
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        MIDISlot &s = configManager.getSlot(i);
        if (s.type != MIDIMessageType::CC)           return false;
        if (s.midiChannel != (uint8_t)((i % 16) + 1)) return false;
        if (s.data1 != i)                            return false;
        if (s.efIndex != (uint8_t)(i % 6))           return false;
        if (!s.active)                               return false;
    }
    return true;
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    delay(200);

    uint8_t flag = EEPROM.read(EEPROM_TEST_FLAG_ADDR);
    if (flag == TEST_NOT_STARTED) {
        Serial.println("EEPROM Test Stage 1: writing data");
        fillTestData();
        configManager.saveConfiguration();
        configManager.saveMIDISlots(testSlots, NUM_SLOTS);
        EEPROM.update(EEPROM_TEST_FLAG_ADDR, TEST_STAGE_SAVE);
        Serial.println("Data written. Please reset the board.");
        return;
    }

    configManager.begin(potChannels);
    configManager.loadMIDISlots(testSlots, NUM_SLOTS);

    if (flag == TEST_STAGE_SAVE) {
        Serial.println("EEPROM Test Stage 2: verifying after reboot");
        if (verifyTestData()) {
            Serial.println("Primary load PASS");
            // corrupt primary magic to force backup usage on next boot
            EEPROM.update(EEPROM_MAGIC_ADDRESS, 0x00);
            EEPROM.update(EEPROM_MAGIC_ADDRESS+1, 0x00);
            EEPROM.update(EEPROM_TEST_FLAG_ADDR, TEST_STAGE_VERIFY);
            Serial.println("Corrupted primary. Reset once more to test backup.");
            delay(1000);
            Utility::rebootTeensy();
        } else {
            Serial.println("Primary load FAIL");
        }
        return;
    }

    if (flag == TEST_STAGE_VERIFY) {
        Serial.println("EEPROM Test Stage 3: verifying backup restore");
        if (verifyTestData()) {
            Serial.println("Backup restore PASS");
        } else {
            Serial.println("Backup restore FAIL");
        }
        EEPROM.update(EEPROM_TEST_FLAG_ADDR, TEST_NOT_STARTED);
    }
}

void loop() {}

