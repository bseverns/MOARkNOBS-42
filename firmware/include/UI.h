#ifndef UI_H
#define UI_H

#include "ButtonManager.h"

void updateFilterTuning(ButtonManagerContext &context);
void updateArpTuning();
void updateNoteDynamics();
void updateControlUi(ButtonManagerContext &context);
bool renderControlOverlayIfActive();
void streamWebSerialState();

void initializeUI();
bool runStartupSequenceStep();
bool isStartupSequenceActive();

#endif // UI_H
