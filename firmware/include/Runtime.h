#ifndef RUNTIME_H
#define RUNTIME_H

#include <stdint.h>
#include <stddef.h>
#include "FirmwareState.h"
#include "SlotModulationResolver.h"

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
void processSlotModulation();
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

// Copy the most recent control-rate resolver trace for host telemetry.
bool getSlotModulationResult(uint8_t slotIndex, SlotModulationResult &result);

#if defined(UNIT_TEST)
void testOnly_resetRuntimeState();
void testOnly_emitClockedSlots(uint32_t quarterEvents);
uint16_t testOnly_modulationTransportBytes();
size_t testOnly_pendingNoteOffCount();
void testOnly_setSlotLfoFrame(uint8_t slotIndex, uint8_t lfoIndex, uint8_t value);
uint8_t testOnly_slotLastEmittedValue(uint8_t slotIndex);
#endif

#endif // RUNTIME_H
