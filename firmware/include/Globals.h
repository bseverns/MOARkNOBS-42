#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <vector>
#include <map>

class ConfigManager;
extern ConfigManager configManager;

class MIDIHandler;
extern MIDIHandler midiHandler;


inline constexpr uint8_t  LED_PIN         = 6;    //!< WS2812 LED data pin
inline constexpr uint8_t  STATUS_LED_PIN  = 23;   //!< Board status indicator
inline constexpr uint8_t  PIN_ROW_DRV     = 7;    //!< Output driver for button rows

inline constexpr uint16_t SLOT_LED_COUNT  = 42;   //!< LEDs mapped to virtual slots
inline constexpr uint8_t  EF_LED_COUNT    = 6;    //!< Envelope follower indicators
inline constexpr uint8_t  POT_LED_COUNT   = 3;    //!< Physical pot indicators
inline constexpr uint16_t EF_LED_OFFSET   = SLOT_LED_COUNT;
inline constexpr uint16_t CONTROL_LED_INDEX = EF_LED_OFFSET + EF_LED_COUNT;
inline constexpr uint16_t POT_LED_OFFSET  = CONTROL_LED_INDEX + 1;
inline constexpr uint16_t NUM_LEDS        = SLOT_LED_COUNT + EF_LED_COUNT + 1 + POT_LED_COUNT; //!< Total LED count
inline constexpr uint8_t  NUM_BUTTONS     = 6;    //!< Number of direct control buttons
inline constexpr uint16_t OLED_WIDTH      = 128;  //!< OLED display width in pixels
inline constexpr uint16_t OLED_HEIGHT     = 64;   //!< OLED display height in pixels
inline constexpr uint8_t  SSD1306_I2C_ADDRESS   = 0x3C; //!< I2C address for the OLED
inline constexpr uint16_t SERIAL_BUFFER_SIZE    = 128;  //!< bytes in the serial buffer
inline constexpr uint8_t  MIDI_TASK_INTERVAL    = 1;    //!< Scheduler tick for MIDI (ms)
inline constexpr uint8_t  SERIAL_TASK_INTERVAL  = 10;   //!< Scheduler tick for serial (ms)
inline constexpr uint8_t  LED_TASK_INTERVAL     = 50;   //!< LED update interval (ms)
inline constexpr uint8_t  ENVELOPE_TASK_INTERVAL = 5;   //!< Envelope follower interval (ms)
inline constexpr uint16_t EEPROM_FILTER_FREQ    = 1000; //!< EEPROM address for filter freq
inline constexpr uint16_t EEPROM_FILTER_Q       = 1004; //!< EEPROM address for filter Q
inline constexpr uint8_t  POT_RANGE_MIN         = 10;   //!< Min pot delta before acting
inline constexpr uint8_t  ENV_RANGE_MIN         = 5;    //!< Min envelope delta threshold

static const uint8_t buttonMuxAnalogPin = A4;
static const uint8_t potMuxAnalogPin    = A5;
// Analog pin tied to the mid-rail reference divider
static const uint8_t VREF_ADC_PIN       = A8;

// ADC scaling from raw reading to volts (3.3V reference, 10-bit ADC)
constexpr float VadcScale = 3.3f / 1023.0f;
// Global storage for measured VREF voltage
extern float g_vref;

// EEPROM storage constants
constexpr uint16_t EEPROM_SLOT_BASE = 0x000; 
constexpr uint8_t SLOT_EEPROM_SIZE = 6;  // bytes required to store a MIDISlot

//clock
constexpr unsigned long CLOCK_TIMEOUT_MS = 2000; // 2 seconds without clock => fallback
extern float g_tappedBPM;

// Pin assignments for primary and secondary mux layers
// The BTN_42 PCB uses CD74HC4067 multiplexers which require four connections to select
// muxR select lines -> pins 2,3,4,5
// muxC select lines -> pins 8,9,10,11
inline constexpr uint8_t MUXR_PINS[4]       = {2, 3, 4, 5};
inline constexpr uint8_t MUXC_PINS[4]       = {8, 9, 10, 11};
inline constexpr uint8_t primaryMuxPins[]   = {2, 3, 4, 5};
inline constexpr uint8_t secondaryMuxPins[] = {8, 9, 10, 11};

// Aliases used by the row-driven button scanner
#define PIN_MUXR primaryMuxPins
#define PIN_MUXC secondaryMuxPins
#define PIN_COL_SENSE buttonMuxAnalogPin

// Direct-wired control buttons use separate GPIOs so they don't
// interfere with the mux select lines.
// Wiring: C0->12, C1->13, C2->14, C3->15, C4->24, C5->25

extern int NORMAL_DISPLAY_TIME;
extern int SHORT_DISPLAY_TIME;

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
