#pragma once
#include <Arduino.h>

// Row driver output pin (common pin of the row multiplexer)
constexpr uint8_t PIN_ROW_DRV = 7;
// Column sense input pin (analog read from the column multiplexer)
constexpr uint8_t PIN_COL_SENSE = A4;

// Multiplexer select lines
constexpr uint8_t PIN_MUXR[4] = {2, 3, 4, 5};
constexpr uint8_t PIN_MUXC[4] = {8, 9, 10, 11};
