#include "Scheduler.h"

#include "CommandQueue.h"
#include "Arpeggiator.h"
#include "FirmwareState.h"
#include "MIDIHandler.h"
#include "Protocol.h"
#include "Runtime.h"
#include "UI.h"
#include "Utility.h"
#include "interop/SeedBoxLink.h"

// Register the recurring task tiers that keep transport, DSP, UI, and interop in balance.
void initializeSchedulers() {
    // High-priority timing: keep the MIDI & DSP work snappy without blocking lower tiers.
    // Order high-tier tasks so transport/clock service happens before modulation/render passes.
    Utility::schedulerHigh.addTask(processMIDI, hwConfig.midiTaskInterval, true);
    Utility::schedulerHigh.addTask(processLFOs, 1, true);
    Utility::schedulerHigh.addTask(processEnvelopeFollowers, 1, true);
    Utility::schedulerHigh.addTask(processPendingNoteOffs, 1, true);
    // Periodic high-priority drain ensures note-off queue never starves under load.
    Utility::schedulerHigh.addTask(
        []() {
            // Drain any accumulated note-offs that missed their window during heavy MIDI bursts.
            processPendingNoteOffs();
        },
        5, true);
    // MIDI serial queue drain at 1ms prevents starvation during quiet periods.
    Utility::schedulerHigh.addTask([]() { midiHandler.serviceSerialQueuePublic(); }, 1, true);
    Utility::schedulerHigh.addTask(
        []() {
            if (now() - lastClockTime > CLOCK_TIMEOUT_MS)
                processInternalClock();
        },
        hwConfig.midiTaskInterval, true);
    Utility::schedulerHigh.addTask(
        []() { arpeggiator.update(midiHandler, configManager, potentiometerManager); },
        hwConfig.midiTaskInterval, true);

    // Mid-priority work: build the command queue, parse JSON/RPC payloads, and keep envelopes
    // polished.
    Utility::schedulerMid.addTask(pollSerialInput, hwConfig.serialTaskInterval, true);
    Utility::schedulerMid.addTask(processCommandQueue, hwConfig.serialTaskInterval, true);
    Utility::schedulerMid.addTask(processEnvelopes, hwConfig.envelopeTaskInterval, true);

    // Throttle hardware scanning to prevent USB starvation on the unthrottled main loop.
    Utility::schedulerMid.addTask(
        []() {
            buttonManager.processButtons(buttonContext);
            potentiometerManager.processPots(ledAnimator, envelopeFollowers);
        },
        2, true);

    // Low-priority visual updates, diagnostics, and WebSerial telemetry.
    Utility::schedulerLow.addTask(
        []() {
            if (isStartupSequenceActive()) {
                return;
            }
            bool clockTick = midiHandler.isClockTick();
            if (clockTick) {
                midiHandler.clearClockTick();
            }
            ledAnimator.tick(now(), clockTick, diagnosticMode);
            ledManager.update();
            updateControlUi(buttonContext);
        },
        hwConfig.ledTaskInterval, true);

    Utility::schedulerLow.addTask(
        []() {
            if (runStartupSequenceStep()) {
                return;
            }
            if (displayManager.isStatusOverlayActive()) {
                return;
            }
            displayManager.beginDraw();
            if (diagnosticMode) {
                const SystemDiagnostics diagSnapshot = captureDiagnosticsSnapshot();
                displayManager.showDiagnostic(diagnosticPage, buttonManager, buttonContext,
                                              midiHandler, diagSnapshot);
                displayManager.endDraw();
                return;
            }
            if (renderControlOverlayIfActive()) {
                displayManager.endDraw();
                return;
            }
            if (displayManager.shouldRunScreensaver()) {
                displayManager.runIdleScreensaver();
                displayManager.endDraw();
                return;
            }
            displayManager.updateFromContext(buttonContext);
            auto it = potToEnvelopeMap.find(buttonContext.activePot);
            if (it != potToEnvelopeMap.end()) {
                displayManager.showEnvelopeLevel(efVoices[buttonContext.activePot].latestLevel());
                int follower = it->second.followerIndex;
                if (follower >= 0 && follower < static_cast<int>(envelopeFollowers.size())) {
                    displayManager.showEnvelopeLevel(
                        envelopeFollowers[follower].getEnvelopeLevel());
                }
            }
            displayManager.highlightActivePot(buttonContext.activePot);
            displayManager.highlightActiveMode(envelopeMode);
            displayManager.endDraw();
        },
        50, true);

    // Persistence flush runs at 100ms to batch filter tuning writes and avoid flash churn.
    Utility::schedulerLow.addTask([]() { flushPendingFilterPersists(); }, 100, true);

    // SeedBox bridge runs slow on purpose; keep interop chatter off the critical paths.
    Utility::schedulerLow.addTask(
        []() { seedbox::interop::mn42::SeedBoxLink::instance().update(); }, 500, true);

    // WebSerial stream cadence targets UI responsiveness without saturating USB serial output.
    Utility::schedulerLow.addTask(streamWebSerialState, 100, true);
}
