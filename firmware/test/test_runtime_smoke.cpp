#include "unity_config.h"
#include <unity.h>

#include "EnvelopeFollower.h"
#include "Hardware/IO.h"
#include "LFO/LFO.h"
#include "TimeStub.h"
#include <cstdint>

extern "C" char *sbrk(int);

namespace {
int g_smokeAnalog = 0;

int analogProvider(uint8_t) { return g_smokeAnalog; }

uintptr_t heapBreak() { return reinterpret_cast<uintptr_t>(sbrk(0)); }
} // namespace

void test_no_heap_growth_over_fake_runtime() {
    // 60 seconds of simulated runtime should not grow the heap.
    hardware::setAnalogReadProvider(analogProvider);
    g_fakeNowMs = 0;

    LFO lfo;
    lfo.setFrequencyHz(0.5f);
    lfo.setDepth(1.0f);
    lfo.setBipolar(true);

    EnvelopeFollower ef(A0, nullptr, 0);
    ef.setVref(1.65f);

    uintptr_t heapStart = heapBreak();

    for (unsigned long ms = 0; ms < 60000; ++ms) {
        g_smokeAnalog = (ms & 1u) ? 512 : 520;
        advanceMs(1);
        ef.update();
        lfo.advanceFreeRun(0.001f);
        (void)lfo.value();
    }

    uintptr_t heapEnd = heapBreak();
    hardware::resetAnalogReadProvider();

    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(heapStart),
                             static_cast<uint32_t>(heapEnd));
}
