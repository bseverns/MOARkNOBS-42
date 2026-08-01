#include "unity_config.h"
#include <unity.h>

#include "ConfigManager.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "Log.h"
#include "Protocol.h"

#include <ArduinoJson.h>

namespace {

String latestLogLine() {
    const String &buffer = peekTestLogBuffer();
    int end = buffer.length();
    while (end > 0 && (buffer[end - 1] == '\n' || buffer[end - 1] == '\r')) {
        --end;
    }
    int start = end - 1;
    while (start >= 0 && buffer[start] != '\n' && buffer[start] != '\r') {
        --start;
    }
    return buffer.substring(start + 1, end);
}

template <size_t Capacity>
void dispatchAndParseJson(const String &command, StaticJsonDocument<Capacity> &doc) {
    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand(command));
    const String line = latestLogLine();
    TEST_ASSERT_TRUE_MESSAGE(line.length() > 0, command.c_str());
    const DeserializationError error = deserializeJson(doc, line);
    TEST_ASSERT_FALSE_MESSAGE(error, line.c_str());
}

} // namespace

void test_profile_storage_commands_restore_and_reset_live_state() {
    StaticJsonDocument<256> response;

    g_activeProfile = 0;
    configManager.setActiveProfile(0);
    configManager.setPotChannel(0, 2);
    configManager.setPotCCNumber(0, 74);
    configManager.getSlot(0).arg.enabled = 1;
    configManager.getSlot(0).arg.method = ARGMethod::MAXX;
    configManager.getSlot(0).arg.sourceA = 3;
    configManager.getSlot(0).arg.sourceB = 5;
    configManager.getSlot(0).lfo.lfo[0].setEnabled(true);
    configManager.getSlot(0).lfo.lfo[0].setMode(ModCombineMode::Centered);
    configManager.getSlot(0).lfo.lfo[0].amount = 46;

    dispatchAndParseJson("SAVE_PROFILE,1", response);
    TEST_ASSERT_TRUE(response["profile_saved"].as<bool>());
    TEST_ASSERT_EQUAL_INT(1, response["profile"].as<int>());
    TEST_ASSERT_EQUAL_UINT8(1, configManager.getActiveProfile());

    configManager.setPotChannel(0, 5);
    configManager.setPotCCNumber(0, 11);
    configManager.getSlot(0).arg = SlotARGConfig{};
    configManager.getSlot(0).lfo = SlotLfoConfig{};

    response.clear();
    dispatchAndParseJson("LOAD_PROFILE,1", response);
    TEST_ASSERT_TRUE(response["profile_loaded"].as<bool>());
    TEST_ASSERT_EQUAL_UINT8(1, configManager.getActiveProfile());
    TEST_ASSERT_EQUAL_UINT8(2, configManager.getPotChannel(0));
    TEST_ASSERT_EQUAL_UINT8(74, configManager.getPotCCNumber(0));
    TEST_ASSERT_TRUE(configManager.getSlot(0).arg.enabled);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ARGMethod::MAXX),
                            static_cast<uint8_t>(configManager.getSlot(0).arg.method));
    TEST_ASSERT_EQUAL_UINT8(3, configManager.getSlot(0).arg.sourceA);
    TEST_ASSERT_EQUAL_UINT8(5, configManager.getSlot(0).arg.sourceB);
    TEST_ASSERT_TRUE(configManager.getSlot(0).lfo.lfo[0].enabled());
    TEST_ASSERT_EQUAL_INT8(46, configManager.getSlot(0).lfo.lfo[0].amount);

    response.clear();
    dispatchAndParseJson("RESET_PROFILE,2", response);
    TEST_ASSERT_TRUE(response["profile_reset"].as<bool>());
    TEST_ASSERT_EQUAL_UINT8(2, configManager.getActiveProfile());
    TEST_ASSERT_EQUAL_UINT8(1, configManager.getPotChannel(0));
    TEST_ASSERT_EQUAL_UINT8(0, configManager.getPotCCNumber(0));
    TEST_ASSERT_FALSE(configManager.getSlot(0).arg.enabled);
    TEST_ASSERT_FALSE(configManager.getSlot(0).lfo.lfo[0].enabled());
}

void test_macro_and_scene_storage_commands_report_inventory_and_restore_state() {
    StaticJsonDocument<768> response;

    configManager.setPotChannel(0, 3);
    configManager.setPotCCNumber(0, 91);
    dispatchAndParseJson("SAVE_MACRO_SLOT", response);
    TEST_ASSERT_TRUE(response["macro_saved"].as<bool>());
    TEST_ASSERT_TRUE(response["macro_available"].as<bool>());

    configManager.setPotChannel(0, 9);
    configManager.setPotCCNumber(0, 12);
    response.clear();
    dispatchAndParseJson("RECALL_MACRO_SLOT", response);
    TEST_ASSERT_TRUE(response["macro_recalled"].as<bool>());
    TEST_ASSERT_TRUE(response["macro_available"].as<bool>());
    TEST_ASSERT_EQUAL_UINT8(3, configManager.getPotChannel(0));
    TEST_ASSERT_EQUAL_UINT8(91, configManager.getPotCCNumber(0));

    response.clear();
    dispatchAndParseJson("{\"cmd\":\"SAVE_SCENE\",\"slot\":0,\"name\":\"Bench\"}", response);
    TEST_ASSERT_TRUE(response["scene_saved"].as<bool>());
    TEST_ASSERT_TRUE(response["scene_available"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("Bench", response["scene_name"] | "");

    response.clear();
    dispatchAndParseJson("{\"cmd\":\"GET_SCENES\"}", response);
    TEST_ASSERT_EQUAL_STRING("GET_SCENES", response["cmd"] | "");
    JsonArray scenes = response["scenes"].as<JsonArray>();
    TEST_ASSERT_TRUE(scenes.size() >= 1);
    TEST_ASSERT_EQUAL_INT(0, scenes[0]["slot"].as<int>());
    TEST_ASSERT_TRUE(scenes[0]["available"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("Bench", scenes[0]["name"] | "");

    configManager.setPotChannel(0, 6);
    configManager.setPotCCNumber(0, 21);
    response.clear();
    dispatchAndParseJson("{\"cmd\":\"RECALL_SCENE\",\"slot\":0}", response);
    TEST_ASSERT_TRUE(response["scene_recalled"].as<bool>());
    TEST_ASSERT_TRUE(response["scene_available"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("Bench", response["scene_name"] | "");
    TEST_ASSERT_EQUAL_UINT8(3, configManager.getPotChannel(0));
    TEST_ASSERT_EQUAL_UINT8(91, configManager.getPotCCNumber(0));
}
