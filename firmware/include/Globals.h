#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <vector>
#include <map>

class ConfigManager;
extern ConfigManager configManager;

class MIDIHandler;
extern MIDIHandler midiHandler;

extern constexpr uint8_t  LED_PIN;           //!< WS2812 LED data pin
extern constexpr uint8_t  STATUS_LED_PIN;    //!< single debug/status LED
extern constexpr uint16_t NUM_LEDS;          //!< Number of addressable LEDs
extern constexpr uint8_t  NUM_BUTTONS;       //!< Number of direct control buttons
extern constexpr uint16_t OLED_WIDTH;        //!< OLED display width in pixels
extern constexpr uint16_t OLED_HEIGHT;       //!< OLED display height in pixels
extern constexpr uint8_t  SSD1306_I2C_ADDRESS; //!< I2C address for the OLED
extern constexpr uint16_t SERIAL_BUFFER_SIZE;   //!< bytes in the serial buffer
extern constexpr uint8_t  MIDI_TASK_INTERVAL;   //!< Scheduler tick for MIDI (ms)
extern constexpr uint8_t  SERIAL_TASK_INTERVAL; //!< Scheduler tick for serial (ms)
extern constexpr uint8_t  LED_TASK_INTERVAL;    //!< LED update interval (ms)
extern constexpr uint8_t  ENVELOPE_TASK_INTERVAL; //!< Envelope follower interval (ms)
extern constexpr uint16_t EEPROM_FILTER_FREQ; //!< EEPROM address for filter freq
extern constexpr uint16_t EEPROM_FILTER_Q;    //!< EEPROM address for filter Q
extern constexpr uint8_t  POT_RANGE_MIN;      //!< Min pot delta before acting
extern constexpr uint8_t  ENV_RANGE_MIN;      //!< Min envelope delta threshold
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
extern const uint8_t MUXR_PINS[4];
extern const uint8_t MUXC_PINS[4];
extern const uint8_t primaryMuxPins[];
extern const uint8_t secondaryMuxPins[];

// Aliases used by the row-driven button scanner
#define PIN_MUXR primaryMuxPins
#define PIN_MUXC secondaryMuxPins
#define PIN_COL_SENSE buttonMuxAnalogPin
extern constexpr uint8_t PIN_ROW_DRV;  //!< Output driver for button rows

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
