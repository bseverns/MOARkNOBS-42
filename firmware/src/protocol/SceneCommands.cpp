#include "protocol/SceneCommands.h"

#include <ArduinoJson.h>
#include <cstring>

#include "Log.h"
#include "protocol/SceneStorage.h"

// SceneCommands.cpp is the structured JSON front door for scene operations.
//
// Reading order:
// 1. tiny JSON response helper
// 2. JSON command decoder
// 3. command fan-out into scene listing, save, and recall operations

template <size_t Capacity> static void sendJsonResponse(const StaticJsonDocument<Capacity> &doc) {
    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

bool handleSceneJsonCommand(const String &command) {
    if (command.length() == 0 || command[0] != '{') {
        return false;
    }
    StaticJsonDocument<512> request;
    DeserializationError err = deserializeJson(request, command);
    if (err) {
        return false;
    }
    const char *cmd = request["cmd"] | nullptr;
    if (!cmd) {
        return false;
    }

    if (std::strcmp(cmd, "GET_SCENES") == 0) {
        SceneStorage::SceneInfo scenes[SceneStorage::kSceneSlotCount];
        uint8_t count = SceneStorage::listScenes(scenes, SceneStorage::kSceneSlotCount);
        StaticJsonDocument<768> response;
        response["cmd"] = "GET_SCENES";
        JsonArray array = response.createNestedArray("scenes");
        for (uint8_t idx = 0; idx < count; ++idx) {
            JsonObject scene = array.createNestedObject();
            scene["slot"] = scenes[idx].slot;
            scene["name"] = scenes[idx].name;
            scene["available"] = scenes[idx].available;
        }
        sendJsonResponse(response);
        return true;
    }

    if (std::strcmp(cmd, "SAVE_SCENE") == 0 || std::strcmp(cmd, "RECALL_SCENE") == 0) {
        const int slotValue = request["slot"].is<int>() ? request["slot"].as<int>() : -1;
        if (slotValue < 0 || slotValue >= SceneStorage::kSceneSlotCount) {
            StaticJsonDocument<256> response;
            response["cmd"] = cmd;
            response["scene_slot"] = slotValue;
            response["success"] = false;
            response["scene_error"] = "Invalid slot";
            sendJsonResponse(response);
            return true;
        }

        if (std::strcmp(cmd, "SAVE_SCENE") == 0) {
            const char *name = request["name"] | nullptr;
            SceneStorage::ConfigState snapshot = SceneStorage::captureConfigState();
            bool saved =
                SceneStorage::saveSceneSlot(static_cast<uint8_t>(slotValue), snapshot, name);
            SceneStorage::SceneEntry entry{};
            SceneStorage::loadSceneSlot(static_cast<uint8_t>(slotValue), entry);
            StaticJsonDocument<384> response;
            response["cmd"] = "SAVE_SCENE";
            response["scene_saved"] = saved;
            response["scene_slot"] = slotValue;
            response["scene_name"] = entry.name;
            response["scene_available"] =
                SceneStorage::sceneSlotAvailable(static_cast<uint8_t>(slotValue));
            if (!saved) {
                response["scene_error"] = "Snapshot save failed";
            }
            sendJsonResponse(response);
            return true;
        }

        SceneStorage::SceneEntry entry{};
        bool loaded = SceneStorage::loadSceneSlot(static_cast<uint8_t>(slotValue), entry);
        StaticJsonDocument<384> response;
        response["cmd"] = "RECALL_SCENE";
        response["scene_slot"] = slotValue;
        response["scene_name"] = entry.name;
        response["scene_available"] = loaded;
        if (loaded) {
            response["scene_recalled"] = SceneStorage::applyConfigState(entry.state, true);
        } else {
            response["scene_recalled"] = false;
            response["scene_error"] = "No snapshot stored in this slot";
        }
        sendJsonResponse(response);
        return true;
    }

    return false;
}
