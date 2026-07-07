#include <Arduino.h>
#include "Globals.h"
#include "Utility.h"
#include "ConfigManager.h"
#include "LEDManager.h"

// Multi-stage test that verifies configuration data survives reboots and that
// the backup storage region can restore corrupted primary data.
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"
#include "EnvelopeFollower.h"
#include "TestHelpers.h"

/*
Storage Persistence Test

Exercises configuration save/restore across reboots using the same ConfigManager
layer pulled in by `FirmwareState.cpp`, so it mirrors the persistence path that
`initializeModes()` walks during a normal boot.

The sketch runs in three manual stages:
  1. Write known data and prompt a reboot.
  2. Verify data after reboot, corrupt the primary copy, reboot again.
  3. Ensure the backup copy restores correctly.

Build with PlatformIO environment `teensy40_eeprom_persistence`
(e.g. `platformio run -e teensy40_eeprom_persistence -t upload`).
Requires a Teensy 4.0 wired as described in the MOARkNOBS hardware docs.

See `firmware/README.md` under "Test Philosophy (and Real Talk)" for
more background on the test suite.
*/

// -- Constants ---------------------------------------------------------------
// address well outside normal config space
static constexpr int EEPROM_TEST_FLAG_ADDR = EEPROM_BACKUP_START + 100;
static constexpr uint8_t TEST_NOT_STARTED = 0xFF;
static constexpr uint8_t TEST_STAGE_SAVE = 0x55;
static constexpr uint8_t TEST_STAGE_VERIFY = 0xAA;

// -- Globals -----------------------------------------------------------------
std::vector<uint8_t> potChannels;
ConfigManager configManager = createConfigManager();
LEDManager ledManager = createLEDManager();
DisplayManager displayManager = createDisplayManager();
PotentiometerManager potentiometerManager = createPotentiometerManager();
ButtonManager buttonManager = createButtonManager(&potentiometerManager);
std::vector<EnvelopeFollower> envelopeFollowers = createEnvelopeFollowers(&potentiometerManager);

static MIDISlot testSlots[NUM_SLOTS];

static StorageBackend *activeStorage() { return ConfigManager::getStorageBackend(); }

static uint8_t readStorageByte(int address) { return activeStorage()->read(address); }

static void writeStorageByte(int address, uint8_t value) {
    activeStorage()->update(address, value);
}

// -----------------------------------------------------------------------------
void fillTestData() {
    for (uint8_t i = 0; i < NUM_POTS; ++i) {
        configManager.setPotChannel(i, (i % 16) + 1);
        configManager.setPotCCNumber(i, 10 + i);
        potentiometerManager.setChannel(i, (i % 16) + 1);
        potentiometerManager.setCCNumber(i, 10 + i);
    }
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        testSlots[i] = {};
        testSlots[i].type = MIDIMessageType::CC;
        testSlots[i].midiChannel = static_cast<uint8_t>((i % 16) + 1);
        testSlots[i].data1 = i;
        testSlots[i].active = true;
        testSlots[i].arpNote = 60;
        testSlots[i].ef.followerIndex = static_cast<int8_t>(i % 6);
        testSlots[i].sysexLength = 0;
        testSlots[i].sysexTemplate.fill(0);
    }
}

bool verifyTestData() {
    for (uint8_t i = 0; i < NUM_POTS; ++i) {
        if (configManager.getPotChannel(i) != (i % 16) + 1)
            return false;
        if (configManager.getPotCCNumber(i) != 10 + i)
            return false;
    }
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        MIDISlot &s = configManager.getSlot(i);
        if (s.type != MIDIMessageType::CC)
            return false;
        if (s.midiChannel != (uint8_t)((i % 16) + 1))
            return false;
        if (s.data1 != i)
            return false;
        if (s.ef.followerIndex != static_cast<int8_t>(i % 6))
            return false;
        if (!s.active)
            return false;
        if (s.arpNote != 60)
            return false;
        if (s.sysexLength != 0)
            return false;
    }
    return true;
}

// Mirror messages to both USB and UART so you can spy on the test from any port
static void logLine(const char *msg) {
    Serial.println(msg);
    Serial1.println(msg);
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    Serial1.begin(SERIAL_BAUD);
    while (!Serial)
        ;
    delay(200);

    uint8_t flag = readStorageByte(EEPROM_TEST_FLAG_ADDR);
    if (flag == TEST_NOT_STARTED) {
        logLine("Storage Test Stage 1: writing data");
        fillTestData();
        configManager.saveConfiguration();
        configManager.saveMIDISlots(testSlots, NUM_SLOTS);
        writeStorageByte(EEPROM_TEST_FLAG_ADDR, TEST_STAGE_SAVE);
        logLine("Data written. Please reset the board.");
        return;
    }

    configManager.begin(potChannels);
    configManager.loadMIDISlots(testSlots, NUM_SLOTS);

    if (flag == TEST_STAGE_SAVE) {
        logLine("Storage Test Stage 2: verifying after reboot");
        if (verifyTestData()) {
            logLine("Primary load PASS");
            // corrupt primary magic to force backup usage on next boot
            writeStorageByte(EEPROM_MAGIC_ADDRESS, 0x00);
            writeStorageByte(EEPROM_MAGIC_ADDRESS + 1, 0x00);
            writeStorageByte(EEPROM_TEST_FLAG_ADDR, TEST_STAGE_VERIFY);
            logLine("Corrupted primary. Reset once more to test backup.");
            delay(1000);
            Utility::rebootTeensy();
        } else {
            logLine("Primary load FAIL");
        }
        return;
    }

    if (flag == TEST_STAGE_VERIFY) {
        logLine("Storage Test Stage 3: verifying backup restore");
        if (verifyTestData()) {
            logLine("Backup restore PASS");
        } else {
            logLine("Backup restore FAIL");
        }
        writeStorageByte(EEPROM_TEST_FLAG_ADDR, TEST_NOT_STARTED);
    }
}

void loop() {}
