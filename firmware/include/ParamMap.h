#pragma once

#include <cstdint>

/**
 * High-level modulation sources used by the routing layer.
 */
enum class ParamSource : uint8_t {
    SOURCE_NONE = 0, //!< No modulation source
    SOURCE_LFO1,     //!< LFO 1 output
    SOURCE_LFO2,     //!< LFO 2 output
};
