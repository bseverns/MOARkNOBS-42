/*
MIDISlot EEPROM Verification

Writes known data to every slot and reloads it to confirm EEPROM integrity.
It uses the same ConfigManager path that `FirmwareState.cpp` exposes so
you can demonstrate how `initializeModes()` and `initializeRuntime()` rely
on those saved slots.

PASS/FAIL results print to the Serial monitor.

Build with PlatformIO environment `teensy40_slot_verify`
(e.g. `pio run -e teensy40_slot_verify -t upload`). Requires
a Teensy 4.0 with the MOARkNOBS wiring.

See `firmware/README.md` under "Test Philosophy (and Real Talk)" for
additional notes on the test suite.
*/

#include <Arduino.h>
#include "ConfigManager.h"
#include "Globals.h"

// Utility test that writes predictable data to each MIDISlot and reads it back
// to confirm EEPROM integrity.
ConfigManager configManager(NUM_POTS, NUM_BUTTONS);

void setup() {
    Serial.begin(SERIAL_BAUD);
    while (!Serial) {
    }

    // Teensy EEPROM API does not expose clear(); each slot is overwritten below.

    Serial.println("Starting MIDISlot EEPROM test...");

    const StorageBackend *storage = ConfigManager::getStorageBackend();
    const uint16_t eepromBytes = storage ? storage->length() : 0;
    const uint16_t slotBytes = static_cast<uint16_t>(SLOT_EEPROM_SIZE);
    const uint16_t slotBytesAvailable = (eepromBytes > EEPROM_SLOT_BASE)
                                            ? static_cast<uint16_t>(eepromBytes - EEPROM_SLOT_BASE)
                                            : 0;
    const uint8_t slotsAddressable =
        (slotBytes > 0) ? static_cast<uint8_t>(slotBytesAvailable / slotBytes) : 0;
    const uint8_t slotsToVerify = (slotsAddressable < NUM_SLOTS) ? slotsAddressable : NUM_SLOTS;

    Serial.printf("Storage bytes=%u, slotBase=%u, slotSize=%u, addressableSlots=%u/%u\n",
                  eepromBytes, EEPROM_SLOT_BASE, slotBytes, slotsToVerify, NUM_SLOTS);
    if (slotsToVerify < NUM_SLOTS) {
        Serial.printf(
            "WARNING: %u slot(s) exceed EEPROM capacity on this board and cannot persist.\n",
            static_cast<unsigned int>(NUM_SLOTS - slotsToVerify));
    }

    if (slotsToVerify == 0) {
        Serial.println("No slot storage available after EEPROM layout offsets.");
        return;
    }

    // Assign unique data to each slot then save
    for (uint8_t i = 0; i < slotsToVerify; ++i) {
        MIDISlot slot{};
        slot.type = MIDIMessageType::CC;
        slot.midiChannel = static_cast<uint8_t>((i % 16) + 1); // MIDI channel
        slot.data1 = static_cast<uint8_t>(20 + i);             // data1
        slot.active = true;
        slot.arpNote = static_cast<uint8_t>(60 + i);               // arp note
        slot.setEnvelopeFollowerIndex(static_cast<int8_t>(i % 6)); // envelope index
        slot.sysexLength = 0;
        slot.sysexTemplate.fill(0);
        configManager.getSlot(i) = slot;
        configManager.saveSlot(i, slot);
    }

    bool allPass = true;

    // Reload each slot and verify
    for (uint8_t i = 0; i < slotsToVerify; ++i) {
        MIDISlot loaded;
        configManager.loadSlot(i, loaded);

        bool pass = loaded.type == MIDIMessageType::CC &&
                    loaded.midiChannel == static_cast<uint8_t>((i % 16) + 1) &&
                    loaded.data1 == static_cast<uint8_t>(20 + i) &&
                    loaded.ef.followerIndex == static_cast<int8_t>(i % 6) && loaded.active &&
                    loaded.arpNote == static_cast<uint8_t>(60 + i) && loaded.sysexLength == 0;

        Serial.print("Slot ");
        Serial.print(i);
        Serial.println(pass ? " PASS" : " FAIL");
        if (!pass)
            allPass = false;
    }

    if (slotsToVerify < NUM_SLOTS) {
        Serial.printf(
            "Verified %u persisted slot(s); %u slot(s) are not addressable on this target.\n",
            slotsToVerify, static_cast<unsigned int>(NUM_SLOTS - slotsToVerify));
    }
    Serial.println(allPass ? "Addressable slots verified."
                           : "One or more addressable slots failed.");
}

void loop() {}
