#ifndef UI_H
#define UI_H

#include "ButtonManager.h"

void updateFilterTuning(ButtonManagerContext &context);
void updateArpTuning();
void updateNoteDynamics();
void updateControlUi(ButtonManagerContext &context);
bool renderControlOverlayIfActive();
bool renderOnDeviceConfigViewIfActive(const ButtonManagerContext &context);
bool renderLfoTuningViewIfActive();
bool renderJitterTuningViewIfActive();
void streamWebSerialState();
void flushPendingFilterPersists();

void initializeUI();
void serviceDisplayDegradedMode();
bool runStartupSequenceStep();
bool isStartupSequenceActive();

#endif // UI_H
