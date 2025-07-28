#include "Globals.h"

// Global runtime variables
float g_vref       = 1.65f;    // midpoint voltage reference
float g_tappedBPM  = 120.0f;   // last tapped tempo

// --- Pin constants ----------------------------------------------------
constexpr uint8_t LED_PIN         = 6;   // WS2812 data
constexpr uint8_t STATUS_LED_PIN  = 23;  // single debug LED
constexpr uint8_t PIN_ROW_DRV     = 7;   // row driver for button mux

constexpr uint16_t NUM_LEDS       = 42;
constexpr uint8_t  NUM_BUTTONS    = 6;
constexpr uint16_t OLED_WIDTH     = 128;
constexpr uint16_t OLED_HEIGHT    = 64;
constexpr uint8_t  SSD1306_I2C_ADDRESS = 0x3C;
constexpr uint16_t SERIAL_BUFFER_SIZE  = 128;
constexpr uint8_t  MIDI_TASK_INTERVAL  = 1;
constexpr uint8_t  SERIAL_TASK_INTERVAL = 10;
constexpr uint8_t  LED_TASK_INTERVAL    = 50;
constexpr uint8_t  ENVELOPE_TASK_INTERVAL = 5;
constexpr uint16_t EEPROM_FILTER_FREQ = 1000;
constexpr uint16_t EEPROM_FILTER_Q    = 1004;
constexpr uint8_t  POT_RANGE_MIN      = 10;
constexpr uint8_t  ENV_RANGE_MIN      = 5;

// --- Hardware maps ----------------------------------------------------
const uint8_t MUXR_PINS[4]       = {2, 3, 4, 5};
const uint8_t MUXC_PINS[4]       = {8, 9, 10, 11};
const uint8_t primaryMuxPins[]   = {2, 3, 4, 5};
const uint8_t secondaryMuxPins[] = {8, 9, 10, 11};
