#ifndef PROTOCOL_SCENE_COMMANDS_H
#define PROTOCOL_SCENE_COMMANDS_H

#include <Arduino.h>

// SceneCommands is the JSON-command front door for scene operations.
//
// It is intentionally separate from the line-command table because scene
// requests are structured JSON packets that short-circuit before the legacy
// comma-delimited command parsing path.

bool handleSceneJsonCommand(const String &command);

#endif // PROTOCOL_SCENE_COMMANDS_H
