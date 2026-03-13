#ifndef RUNTIME_H
#define RUNTIME_H

#include <stdint.h>
#include <stddef.h>
#include "FirmwareState.h"

void processMIDI();
void processEnvelopeFollowers();
void processLFOs();
void processEnvelopes();
void processPendingNoteOffs();
void processInternalClock();
void monitorSystemLoad();
void monitorSerialHealth();
void initializeRuntime(bool baselinesLoaded);
void midiTimerISR();

bool queuePendingNoteOff(uint8_t note, uint8_t channel, unsigned long delayMs);

void requestStatusLEDPulse(uint16_t durationMs = 200);
void serviceStatusLEDPulse();
void checkDiagnosticsForAlerts();

#if defined(UNIT_TEST)
void testOnly_resetRuntimeState();
#endif

#endif // RUNTIME_H
