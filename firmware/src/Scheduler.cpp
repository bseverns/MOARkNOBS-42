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
    // Update a couple of followers per pass so the full bank refreshes every ~6ms without
    // dropping a multi-millisecond slab onto one loop iteration.
    Utility::schedulerHigh.addTask(processEnvelopeFollowers, 2, true);
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

    // Scan one matrix row plus a pot slice per pass so the full panel still refreshes quickly
    // without monopolizing a single loop iteration.
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
            if (!displayManager.isReady()) {
                return;
            }
            if (runStartupSequenceStep()) {
                return;
            }
            // Let mode-specific views (config/LFO/jitter/diagnostics) remain visible even when
            // status toasts are active, so control feedback is always explicit on OLED.
            if (displayManager.isStatusOverlayActive() &&
                !buttonManager.isOnDeviceConfigModeActive() &&
                !buttonManager.isLfoTuningModeActive() && !g_jitterTuningActive &&
                !diagnosticMode) {
                return;
            }
            displayManager.beginDraw();
            if (renderOnDeviceConfigViewIfActive(buttonContext)) {
                displayManager.endDraw();
                return;
            }
            if (renderLfoTuningViewIfActive()) {
                displayManager.endDraw();
                return;
            }
            if (renderJitterTuningViewIfActive()) {
                displayManager.endDraw();
                return;
            }
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
        []() {
            if (g_seedboxInteropEnabled) {
                seedbox::interop::mn42::SeedBoxLink::instance().update();
            }
        },
        500, true);

    // WebSerial stream cadence targets UI responsiveness without saturating USB serial output.
    Utility::schedulerLow.addTask(streamWebSerialState, 100, true);
}
