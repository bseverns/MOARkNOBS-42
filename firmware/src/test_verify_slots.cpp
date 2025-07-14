#include <Arduino.h>
#include "ConfigManager.h"
#include "Globals.h"

// Simple EEPROM slot verification
ConfigManager configManager(NUM_POTS, NUM_BUTTONS);

void setup() {
    Serial.begin(115200);
    while (!Serial) {}

    Serial.println("Starting MIDISlot EEPROM test...");

    // Assign unique data to each slot then save
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        MIDISlot slot = {
            MIDIMessageType::CC,
            static_cast<uint8_t>((i % 16) + 1), // MIDI channel
            static_cast<uint8_t>(20 + i),       // data1
            static_cast<uint8_t>(i % 6),        // envelope index
            true,
            static_cast<uint8_t>(60 + i)        // arp note
        };
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
                    loaded.efIndex == static_cast<uint8_t>(i % 6) &&
                    loaded.active &&
                    loaded.arpNote == static_cast<uint8_t>(60 + i);

        Serial.print("Slot ");
        Serial.print(i);
        Serial.println(pass ? " PASS" : " FAIL");
        if (!pass) allPass = false;
    }

    Serial.println(allPass ? "All slots verified." : "One or more slots failed.");
}

void loop() {}
