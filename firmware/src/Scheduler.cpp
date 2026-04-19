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
            if (diagnosticMode) {
                displayManager.beginDraw();
                // Show the latest diagnostics snapshot while the UI task is in control.
                const SystemDiagnostics diagSnapshot = captureDiagnosticsSnapshot();
                displayManager.showDiagnostic(diagnosticPage, buttonManager, buttonContext,
                                              midiHandler, diagSnapshot);
                displayManager.endDraw();
            } else if (!displayManager.shouldRunScreensaver()) {
                displayManager.beginDraw();
                displayManager.updateFromContext(buttonContext);
                auto it = potToEnvelopeMap.find(buttonContext.activePot);
                if (it != potToEnvelopeMap.end()) {
                    displayManager.showEnvelopeLevel(
                        efVoices[buttonContext.activePot].latestLevel());
                    int follower = it->second.followerIndex;
                    if (follower >= 0 && follower < static_cast<int>(envelopeFollowers.size())) {
                        displayManager.showEnvelopeLevel(
                            envelopeFollowers[follower].getEnvelopeLevel());
                    }
                }
                displayManager.highlightActivePot(buttonContext.activePot);
                displayManager.highlightActiveMode(envelopeMode);
                displayManager.endDraw();
            } else {
                displayManager.runIdleScreensaver();
            }
        },
        50, true);

    // SeedBox bridge runs slow on purpose; keep interop chatter off the critical paths.
    Utility::schedulerLow.addTask(
        []() { seedbox::interop::mn42::SeedBoxLink::instance().update(); }, 500, true);

    // WebSerial stream cadence targets UI responsiveness without saturating USB serial output.
    Utility::schedulerLow.addTask(streamWebSerialState, 100, true);
}
