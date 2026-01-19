#ifndef UI_H
#define UI_H

#include "ButtonManager.h"

void updateFilterTuning(ButtonManagerContext &context);
void updateArpTuning();
void updateNoteDynamics();
void streamWebSerialState();

void initializeUI();

#endif // UI_H
