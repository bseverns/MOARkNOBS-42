#include <Arduino.h>
#include "ConfigManager.h"

ConfigManager config;

void setup() {
    Serial.begin(115200);
    config.begin();

    // Example slot assignment
    config.slots[0] = {MIDIMessageType::CC, 1, 74, 0, true};
    config.saveSlot(0, config.slots[0]);

    MIDISlot loadedSlot;
    config.loadSlot(0, loadedSlot);

    Serial.println("Slot 0 loaded:");
    Serial.print("Type: "); Serial.println((uint8_t)loadedSlot.type);
    Serial.print("Channel: "); Serial.println(loadedSlot.midiChannel);
    Serial.print("Data1: "); Serial.println(loadedSlot.data1);
    Serial.print("EF Index: "); Serial.println(loadedSlot.efIndex);
    Serial.print("Active: "); Serial.println(loadedSlot.active);
}

void loop() {}
