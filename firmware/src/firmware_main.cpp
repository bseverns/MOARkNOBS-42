#include <Arduino.h>

#include "FirmwareState.h"
#include "Globals.h"
#include "Protocol.h"
#include "Modes.h"
#include "UI.h"
#include "Runtime.h"
#include "Utility.h"

// Firmware bootstrap stays intentionally small: initialize protocol, recover
// persisted/profile state, build the UI, then arm the runtime schedulers.
void setup() {
    initializeProtocol();
    bool baselinesLoaded = initializeModes();
    initializeUI();
    initializeRuntime(baselinesLoaded);
}

// Main loop keeps the hot path readable by delegating the heavy lifting to the
// schedulers and managers initialized during setup.
void loop() {
    Utility::schedulerHigh.update();
    Utility::schedulerMid.update();
    Utility::schedulerLow.update();
    buttonManager.processButtons(buttonContext);
    potentiometerManager.processPots(ledAnimator, envelopeFollowers);
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
