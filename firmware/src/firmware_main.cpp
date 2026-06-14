#include <Arduino.h>

#include "BootMode.h"
#include "DiagnosticRecord.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "Protocol.h"
#include "CommandQueue.h"
#include "Log.h"
#include "Modes.h"
#include "UI.h"
#include "Runtime.h"
#include "Utility.h"

// Reading path for new contributors:
// 1. BootMode.h      -> which personality boots: standalone instrument or USB configurator
// 2. FirmwareState.h -> which long-lived managers and runtime objects exist
// 3. Globals.h       -> hardware constants, EEPROM layout, and shared scalar state
// 4. Protocol.h      -> host/configurator command lane
// 5. Modes.h         -> persisted musical state and profile snapshot logic
// 6. UI.h            -> on-device OLED/button control surface
// 7. Runtime.h       -> scheduled hot path used once the instrument is alive
// 8. Utility.h       -> shared schedulers plus low-level helpers used by the layers above
//
// The dedicated walkthrough for this file lives in docs/firmware/FirmwareMainReadingPath.md.

namespace {
BootMode gBootMode = BootMode::StandaloneRuntime;
bool gFirstStandaloneLoop = true;
} // namespace

// Firmware bootstrap stays intentionally small: initialize protocol, recover
// persisted/profile state, build the UI, then arm the runtime schedulers.
void setup() {
    gBootMode = selectBootMode();
    initializeProtocol();
    DiagnosticRecord::recordBootMode(static_cast<uint8_t>(gBootMode));
#if defined(MN42_DIAG_BOOT_MARKERS) && (MN42_DIAG_BOOT_MARKERS != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"setup_before_load_hardware\"}");
#endif
    loadHardwareConfig();
    if (gBootMode == BootMode::UsbConfigurator) {
        restoreActiveProfileRuntime(false);
        LOG_PRINTLN("{\"type\":\"boot_mode\",\"mode\":\"usb_configurator\"}");
        return;
    }
    LOG_PRINTLN("{\"type\":\"boot_mode\",\"mode\":\"standalone_runtime\"}");
#if defined(MN42_DIAG_BOOT_MARKERS) && (MN42_DIAG_BOOT_MARKERS != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"setup_before_initialize_modes\"}");
#endif
    bool baselinesLoaded = false;
#if defined(MN42_DIAG_DISABLE_MODES_INIT) && (MN42_DIAG_DISABLE_MODES_INIT != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"modes_init_disabled\"}");
#else
    baselinesLoaded = initializeModes();
#endif
#if defined(MN42_DIAG_BOOT_MARKERS) && (MN42_DIAG_BOOT_MARKERS != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"setup_after_initialize_modes\"}");
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"setup_before_initialize_ui\"}");
#endif
#if defined(MN42_DIAG_DISABLE_UI_INIT) && (MN42_DIAG_DISABLE_UI_INIT != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"ui_init_disabled\"}");
#else
    initializeUI();
#endif
#if defined(MN42_DIAG_BOOT_MARKERS) && (MN42_DIAG_BOOT_MARKERS != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"setup_after_initialize_ui\"}");
#endif
#if defined(MN42_DIAG_DISABLE_RUNTIME_INIT) && (MN42_DIAG_DISABLE_RUNTIME_INIT != 0)
    (void)baselinesLoaded;
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"runtime_init_disabled\"}");
#else
#if defined(MN42_DIAG_BOOT_MARKERS) && (MN42_DIAG_BOOT_MARKERS != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"setup_before_initialize_runtime\"}");
#endif
    initializeRuntime(baselinesLoaded);
#if defined(MN42_DIAG_BOOT_MARKERS) && (MN42_DIAG_BOOT_MARKERS != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"setup_after_initialize_runtime\"}");
#endif
#endif
}

// Main loop keeps the hot path readable by delegating the heavy lifting to the
// schedulers and managers initialized during setup.
void loop() {
    if (gBootMode == BootMode::UsbConfigurator) {
        pollSerialInput();
        processCommandQueue();
        return;
    }
#if defined(MN42_DIAG_BOOT_MARKERS) && (MN42_DIAG_BOOT_MARKERS != 0)
    if (gFirstStandaloneLoop) {
        LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"loop_first_entry\"}");
        gFirstStandaloneLoop = false;
    }
#endif
#if defined(MN42_DIAG_DISABLE_STANDALONE_LOOP) && (MN42_DIAG_DISABLE_STANDALONE_LOOP != 0)
    delay(10);
    return;
#endif
    Utility::schedulerHigh.update();
    Utility::schedulerMid.update();
    Utility::schedulerLow.update();
    if (g_profileChangeRequested) {
        ProfileData profile{};
        if (configManager.loadProfileSettings(g_activeProfile, profile)) {
            applyProfileSnapshot(profile, true);
        }
        g_profileChangeRequested = false;
    }
    if (g_profileSaveRequested) {
        ProfileData profile = captureProfileSnapshot();
        configManager.saveProfileSettings(g_activeProfile, profile);
        g_profileSaveRequested = false;
    }
    monitorSystemLoad();
}
