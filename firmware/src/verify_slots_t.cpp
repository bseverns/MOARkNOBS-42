
/*
 * MIDISlot EEPROM Verification
 *
 * Writes known data to every slot and reloads it to confirm EEPROM
 * integrity. PASS/FAIL results print to the Serial monitor.
 *
 * Build with PlatformIO environment `teensy40_slot_verify`
 * (e.g. `platformio run -e teensy40_slot_verify -t upload`). Requires
 * a Teensy 4.0 with the MOARkNOBS wiring.
 *
 * See `firmware/README.md` under "Test Philosophy (and Real Talk)" for
 * additional notes on the test suite.
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

    Serial.println("Starting MIDISlot EEPROM test...");

    // Assign unique data to each slot then save
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        MIDISlot slot = {
            MIDIMessageType::CC,
            static_cast<uint8_t>((i % 16) + 1), // MIDI channel
            static_cast<uint8_t>(20 + i),       // data1
            static_cast<uint8_t>(i % 6),        // envelope index
            true,
            static_cast<uint8_t>(60 + i) // arp note
        };
        slot.sysexLength = 0;
        slot.sysexTemplate.fill(0);
        configManager.getSlot(i) = slot;
        configManager.saveSlot(i, slot);
    }

    bool allPass = true;

    // Reload each slot and verify
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
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

    Serial.println(allPass ? "All slots verified." : "One or more slots failed.");
}

void loop() {}
