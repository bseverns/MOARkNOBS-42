#ifndef BUTTON_INPUT_CONSTANTS_H
#define BUTTON_INPUT_CONSTANTS_H

#include <stdint.h>

inline constexpr uint8_t NUM_VIRTUAL_BUTTONS = 42;
inline constexpr uint8_t NUM_CONTROL_BUTTONS = 6;
inline constexpr uint8_t NUM_BUTTON_INPUTS = NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS;
inline constexpr unsigned long DEBOUNCE_DELAY = 50;
inline constexpr uint8_t BUTTON_ROWS = 7;
inline constexpr uint8_t BUTTON_COLS = 6;
inline constexpr int BUTTON_PRESS_THRESHOLD = 512;

#endif // BUTTON_INPUT_CONSTANTS_H

