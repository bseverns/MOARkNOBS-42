#pragma once
#include <Arduino.h>

// LED strip data pin
constexpr uint8_t LED_PIN = 6;

// Row (R) and column (C) select lines for the button matrix multiplexers
const uint8_t MUXR_PINS[4] = {2, 3, 4, 5}; // MUXR1..4
const uint8_t MUXC_PINS[4] = {8, 9, 10, 11}; // MUXC1..4

// Shared analog nodes
constexpr uint8_t BUTTON_MUX_PIN = A4; // Sense line for button matrix
constexpr uint8_t POT_MUX_PIN    = A5; // Analog read for pot mux

// Direct control buttons
const uint8_t CONTROL_PINS[6] = {12, 13, 14, 15, 24, 25};

