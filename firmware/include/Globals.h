#ifndef GLOBALS_H
#define GLOBALS_H

#include <vector>
#include "ConfigManager.h"

// Constants
#define LED_PIN 6
#define NUM_LEDS 42
#define NUM_BUTTONS 6
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_I2C_ADDRESS 0x3C
#define SERIAL_BUFFER_SIZE 128
#define MIDI_TASK_INTERVAL 1      // 1ms for MIDI processing
#define SERIAL_TASK_INTERVAL 10   // 10ms for Serial processing
#define LED_TASK_INTERVAL 50      // 50ms for LED updates
#define ENVELOPE_TASK_INTERVAL 5  // 5ms for Envelope processing

// Define NUM_POTS globally
#define NUM_POTS 42

// Declare global variables
extern ConfigManager configManager;

#endif // GLOBALS_H
