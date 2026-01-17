#pragma once

#include <cstdint>

/**
 * Supported low-frequency oscillator waveforms.
 * Each shape is rendered in the LFO engine and then scaled by depth/polarity.
 */
enum class LFOShape : uint8_t {
    Sine = 0,   //!< Classic sine wave (-1..1 before depth scaling)
    Triangle,   //!< Linear rise/fall across the cycle
    Saw,        //!< Rising ramp that wraps hard at the cycle boundary
    Square,     //!< High/low gate at 50% duty
    SampleHold, //!< Discrete random value per cycle
    RandomSlew  //!< Random value per cycle with smoothing between steps
};
