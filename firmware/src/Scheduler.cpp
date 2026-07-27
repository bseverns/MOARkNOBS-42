#include "Scheduler.h"

#include "CommandQueue.h"
#include "Arpeggiator.h"
#include "FirmwareState.h"
#include "MIDIHandler.h"
#include "Protocol.h"
#include "protocol/ProtocolSimpleHandlers.h"
#include "Runtime.h"
#include "UI.h"
#include "Utility.h"
#include "Log.h"
#include "interop/SeedBoxLink.h"

// Register the recurring task tiers that keep transport, DSP, UI, and interop in balance.
void initializeSchedulers() {
#if defined(MN42_DIAG_DISABLE_SCHEDULER_HIGH) && (MN42_DIAG_DISABLE_SCHEDULER_HIGH != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"scheduler_high_disabled\"}");
#else
    // High-priority timing: keep the MIDI & DSP work snappy without blocking lower tiers.
    // Order high-tier tasks so transport/clock service happens before modulation/render passes.
#if defined(MN42_DIAG_DISABLE_PROCESS_MIDI) && (MN42_DIAG_DISABLE_PROCESS_MIDI != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"process_midi_disabled\"}");
#else
    Utility::schedulerHigh.addTask(processMIDI, hwConfig.midiTaskInterval, true);
#endif

#if defined(MN42_DIAG_DISABLE_PROCESS_LFOS) && (MN42_DIAG_DISABLE_PROCESS_LFOS != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"process_lfos_disabled\"}");
#else
    Utility::schedulerHigh.addTask(processLFOs, 1, true);
#endif

    // Update a couple of followers per pass so the full bank refreshes every ~6ms without
    // dropping a multi-millisecond slab onto one loop iteration.
#if defined(MN42_DIAG_DISABLE_PROCESS_ENVELOPE_FOLLOWERS) &&                                       \
    (MN42_DIAG_DISABLE_PROCESS_ENVELOPE_FOLLOWERS != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"process_envelope_followers_disabled\"}");
#else
    Utility::schedulerHigh.addTask(processEnvelopeFollowers, 2, true);
#endif

#if defined(MN42_DIAG_DISABLE_PROCESS_PENDING_NOTEOFFS) &&                                         \
    (MN42_DIAG_DISABLE_PROCESS_PENDING_NOTEOFFS != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"process_pending_noteoffs_disabled\"}");
#else
    Utility::schedulerHigh.addTask(processPendingNoteOffs, 1, true);
    // Periodic high-priority drain ensures note-off queue never starves under load.
    Utility::schedulerHigh.addTask(
        []() {
            // Drain any accumulated note-offs that missed their window during heavy MIDI bursts.
            processPendingNoteOffs();
        },
        5, true);
#endif

#if defined(MN42_DIAG_DISABLE_SERVICE_SERIAL_QUEUE) && (MN42_DIAG_DISABLE_SERVICE_SERIAL_QUEUE != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"service_serial_queue_disabled\"}");
#else
    // MIDI serial queue drain at 1ms prevents starvation during quiet periods.
    Utility::schedulerHigh.addTask([]() { midiHandler.serviceSerialQueuePublic(); }, 1, true);
#endif

#if defined(MN42_DIAG_DISABLE_PROCESS_INTERNAL_CLOCK) &&                                           \
    (MN42_DIAG_DISABLE_PROCESS_INTERNAL_CLOCK != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"process_internal_clock_disabled\"}");
#else
    Utility::schedulerHigh.addTask([]() { processInternalClock(); }, hwConfig.midiTaskInterval,
                                   true);
#endif

#if defined(MN42_DIAG_DISABLE_ARPEGGIATOR_UPDATE) && (MN42_DIAG_DISABLE_ARPEGGIATOR_UPDATE != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"arpeggiator_update_disabled\"}");
#else
    Utility::schedulerHigh.addTask(
        []() { arpeggiator.update(midiHandler, configManager, potentiometerManager); },
        hwConfig.midiTaskInterval, true);
#endif
#endif

#if defined(MN42_DIAG_DISABLE_SCHEDULER_MID) && (MN42_DIAG_DISABLE_SCHEDULER_MID != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"scheduler_mid_disabled\"}");
#else
    // Mid-priority work: build the command queue, parse JSON/RPC payloads, and keep envelopes
    // polished.
#if defined(MN42_DIAG_DISABLE_POLL_SERIAL_INPUT) && (MN42_DIAG_DISABLE_POLL_SERIAL_INPUT != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"poll_serial_input_disabled\"}");
#else
    Utility::schedulerMid.addTask(pollSerialInput, hwConfig.serialTaskInterval, true);
#endif

#if defined(MN42_DIAG_DISABLE_PROCESS_COMMAND_QUEUE) &&                                            \
    (MN42_DIAG_DISABLE_PROCESS_COMMAND_QUEUE != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"process_command_queue_disabled\"}");
#else
    Utility::schedulerMid.addTask(processCommandQueue, hwConfig.serialTaskInterval, true);
#endif

#if defined(MN42_DIAG_DISABLE_PROCESS_ENVELOPES) && (MN42_DIAG_DISABLE_PROCESS_ENVELOPES != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"process_envelopes_disabled\"}");
#else
    Utility::schedulerMid.addTask(processEnvelopes, hwConfig.envelopeTaskInterval, true);
#endif

    // Scan one matrix row plus a pot slice per pass so the full panel still refreshes quickly
    // without monopolizing a single loop iteration.
#if defined(MN42_DIAG_DISABLE_PANEL_SCAN) && (MN42_DIAG_DISABLE_PANEL_SCAN != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"panel_scan_disabled\"}");
#else
    Utility::schedulerMid.addTask(
        []() {
            buttonManager.processButtons(buttonContext);
            potentiometerManager.processPots(ledAnimator, envelopeFollowers);
        },
        2, true);
#endif
#endif

#if defined(MN42_DIAG_DISABLE_SCHEDULER_LOW) && (MN42_DIAG_DISABLE_SCHEDULER_LOW != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"scheduler_low_disabled\"}");
#else
    // Low-priority visual updates, diagnostics, and WebSerial telemetry.
    Utility::schedulerLow.addTask(
        []() { ProtocolSimpleHandlers::serviceChunkedReadOutput(); }, 1, true);

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
            serviceDisplayDegradedMode();
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

    // Scope gets compact high-cadence frames; full dashboard snapshots are heavier and slower.
    Utility::schedulerLow.addTask(streamWebSerialScope, 50, true);
    Utility::schedulerLow.addTask(streamWebSerialState, 500, true);
#endif
}
