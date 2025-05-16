#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <vector>
#include <map>
#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include "LEDManager.h"

class ConfigManager;
extern ConfigManager configManager;

#define LED_PIN 6
#define NUM_LEDS 42
#define NUM_BUTTONS 6
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define SSD1306_I2C_ADDRESS 0x3C
#define SERIAL_BUFFER_SIZE 128
#define MIDI_TASK_INTERVAL 1      // 1ms for MIDI processing
#define SERIAL_TASK_INTERVAL 10   // 10ms for Serial processing
#define LED_TASK_INTERVAL 50      // 50ms for LED updates
#define ENVELOPE_TASK_INTERVAL 5  // 5ms for Envelope processing
#define EEPROM_FILTER_FREQ 1000
#define EEPROM_FILTER_Q    1004

#define NUM_POTS 42

//clock
constexpr unsigned long CLOCK_TIMEOUT_MS = 2000; // 2 seconds without clock => fallback
extern float g_tappedBPM;

const uint8_t FILTER_FREQ_POT_PIN = 22;
const uint8_t FILTER_RES_POT_PIN = 23;

// Pin assignments for primary and secondary mux layers
const uint8_t primaryMuxPins[] = {7, 8, 9};
const uint8_t secondaryMuxPins[] = {10, 11, 12};
const uint8_t analogPin = 4; //mux reader

int NORMAL_DISPLAY_TIME = 30000;
int SHORT_DISPLAY_TIME = 10000;

static const std::pair<int,int> ARG_PAIRS[] = {
    // All pairs beginning with A0
    {A0, A1},
    {A0, A2},
    {A0, A3},
    {A0, A6},
    {A0, A7},

    // Then pairs beginning with A1
    {A1, A0},
    {A1, A2},
    {A1, A3},
    {A1, A6},
    {A1, A7},

    // Then pairs beginning with A2
    {A2, A0},
    {A2, A1},
    {A2, A3},
    {A2, A6},
    {A2, A7},

    // Then pairs beginning with A3
    {A3, A0},
    {A3, A1},
    {A3, A2},
    {A3, A6},
    {A3, A7},

    // Finally the one pair from A6
    {A6, A0},
    {A6, A1},
    {A6, A2},
    {A6, A3},
    {A6, A7}
};

#endif // GLOBALS_H