#ifdef UNIT_TEST
#include "DisplayManager.h"

// Stubs for DisplayManager methods called by UI.cpp
// during the software-only unit testing run, where the real
// DisplayManager.cpp is excluded to prevent Adafruit SSD1306
// hardware library compilation.

void DisplayManager::drawFilterTuning(const char *filterName, float freq, const char *targetParam,
                                      float val) {
    (void)filterName;
    (void)freq;
    (void)targetParam;
    (void)val;
}

void DisplayManager::drawArpSettings(uint8_t currentStep, const char *shapeAbbr) {
    (void)currentStep;
    (void)shapeAbbr;
}

void DisplayManager::drawText(const char *line1, const char *line2, const char *line3) {
    (void)line1;
    (void)line2;
    (void)line3;
}
#endif
