#include "Globals.h"

float g_vref = 1.65f;
float g_tappedBPM = 120.0f;

// Hardware pin maps
const uint8_t MUXR_PINS[4]       = {2, 3, 4, 5};
const uint8_t MUXC_PINS[4]       = {8, 9, 10, 11};
const uint8_t primaryMuxPins[]   = {2, 3, 4, 5};
const uint8_t secondaryMuxPins[] = {8, 9, 10, 11};

// LED strip data pin
constexpr uint8_t LED_PIN = 6;
// Single status LED indicator pin
constexpr uint8_t STATUS_LED_PIN = 23;
