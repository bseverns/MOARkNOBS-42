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
bool renderControlHelpIfActive();
void streamWebSerialScope();
void streamWebSerialState();
void flushPendingFilterPersists();
void cancelPendingFilterPersists();
void markFilterTuningRemoteControlActive(uint8_t slotIndex);
void markAllFilterTuningRemoteControlActive();
void clearFilterTuningRemoteControl();

void initializeUI();
void serviceDisplayDegradedMode();
bool runStartupSequenceStep();
bool isStartupSequenceActive();

#endif // UI_H
