#ifndef UI_H
#define UI_H

#include "ButtonManager.h"

void updateFilterTuning(ButtonManagerContext &context, bool renderDisplay = true);
void updateArpTuning(bool renderDisplay = true);
void updateNoteDynamics(bool renderDisplay = true);
void updateControlUi(ButtonManagerContext &context);
void streamWebSerialState();

void initializeUI();
bool runStartupSequenceStep();
bool isStartupSequenceActive();

#endif // UI_H
