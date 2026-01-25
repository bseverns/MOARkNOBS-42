#include <Arduino.h>

#include "FirmwareState.h"
#include "Globals.h"
#include "Protocol.h"
#include "Modes.h"
#include "UI.h"
#include "Runtime.h"
#include "Utility.h"

void setup() {
    initializeProtocol();
    bool baselinesLoaded = initializeModes();
    initializeUI();
    initializeRuntime(baselinesLoaded);
}

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
