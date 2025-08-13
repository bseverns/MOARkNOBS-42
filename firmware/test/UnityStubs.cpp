#include "DisplayManager.h"
#include "TimeUtils.h"
#include "Globals.h"

unsigned long now() { return 0; }

float g_tappedBPM = 0.0f;
bool g_clockOutEnabled = false;
bool g_usbMidiOutEnabled = true;
unsigned long lastClockTime = 0;

void DisplayManager::registerInteraction() {}

