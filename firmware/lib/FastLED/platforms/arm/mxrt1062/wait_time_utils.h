#ifndef __INC_FASTLED_MXRT1062_WAIT_TIME_UTILS_H
#define __INC_FASTLED_MXRT1062_WAIT_TIME_UTILS_H

#include <stdint.h>

namespace fastled_mxrt1062 {
inline int32_t clampWaitTimeDeltaUs(int wait_time_us, int interrupt_threshold_us) {
    int32_t delta = static_cast<int32_t>(wait_time_us) - static_cast<int32_t>(interrupt_threshold_us);
    return delta > 0 ? delta : 0;
}
}  // namespace fastled_mxrt1062

#endif  // __INC_FASTLED_MXRT1062_WAIT_TIME_UTILS_H
