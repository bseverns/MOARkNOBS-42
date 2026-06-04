#ifndef RUNTIME_H
#define RUNTIME_H

#include <stdint.h>
#include <stddef.h>
#include "FirmwareState.h"

// Runtime.h is the "repeating work" header.
//
// If FirmwareState.h lists the actors, Runtime.h lists the recurring jobs that
// keep those actors alive once setup() has finished. Read these declarations as
// scheduler-facing responsibilities rather than as generic helpers.

// Boot-time runtime bring-up.
void initializeRuntime(bool baselinesLoaded);
void midiTimerISR();

// High-frequency service lanes.
void processMIDI();
void processEnvelopeFollowers();
void processLFOs();

// Mid/low-tier musical processing and deferred cleanup.
void processEnvelopes();
void processPendingNoteOffs();
void processInternalClock();

// Diagnostics and runtime health.
void monitorSystemLoad();
void monitorSerialHealth();

// Pending note-off queue used by note-generating lanes.
bool queuePendingNoteOff(uint8_t note, uint8_t channel, unsigned long delayMs);

// Status LED and diagnostic alert surface.
void requestStatusLEDPulse(uint16_t durationMs = 200);
void serviceStatusLEDPulse();
void checkDiagnosticsForAlerts();

#if defined(UNIT_TEST)
void testOnly_resetRuntimeState();
void testOnly_emitClockedSlots(uint32_t quarterEvents);
#endif

#endif // RUNTIME_H
