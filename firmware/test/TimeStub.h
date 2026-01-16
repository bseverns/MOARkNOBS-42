#pragma once

// Shared fake time base for tests that override ::now().
extern unsigned long g_fakeNowMs;
// Advance the fake clock by the given number of milliseconds.
void advanceMs(unsigned long ms);
