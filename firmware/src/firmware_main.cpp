#include <Arduino.h>

#include "BootMode.h"
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
} // namespace

// Firmware bootstrap stays intentionally small: initialize protocol, recover
// persisted/profile state, build the UI, then arm the runtime schedulers.
void setup() {
    gBootMode = selectBootMode();
    initializeProtocol();
    if (gBootMode == BootMode::UsbConfigurator) {
        LOG_PRINTLN("{\"type\":\"boot_mode\",\"mode\":\"usb_configurator\"}");
        return;
    }
    loadHardwareConfig();
    LOG_PRINTLN("{\"type\":\"boot_mode\",\"mode\":\"standalone_runtime\"}");
    bool baselinesLoaded = initializeModes();
    initializeUI();
    initializeRuntime(baselinesLoaded);
}

// Main loop keeps the hot path readable by delegating the heavy lifting to the
// schedulers and managers initialized during setup.
void loop() {
    if (gBootMode == BootMode::UsbConfigurator) {
        pollSerialInput();
        processCommandQueue();
        return;
    }
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
