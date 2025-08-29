#ifndef __INC_FASTLED_ARM_MXRT1062_H
#define __INC_FASTLED_ARM_MXRT1062_H

#include "fastpin_arm_mxrt1062.h"
#include "fastspi_arm_mxrt1062.h"
#include "octows2811_controller.h"
#include "clockless_arm_mxrt1062.h"
#include "block_clockless_arm_mxrt1062.h"

// Teensy 3-only helpers got the boot. If you need WS2812Serial or SmartMatrix,
// bring back the k20 platform from upstream.
#if __has_include("../k20/ws2812serial_controller.h")
#include "../k20/ws2812serial_controller.h"
#endif

#if __has_include("../k20/smartmatrix_t3.h")
#include "../k20/smartmatrix_t3.h"
#endif

#endif
