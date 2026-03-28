#include "unity_config.h"
#include <unity.h>

#include "ConfigManager.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "Protocol.h"

void test_dispatch_handles_known_command() {
    webSerialStreaming = false;
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("HELLO"));
    TEST_ASSERT_TRUE(webSerialStreaming);
}

void test_dispatch_handles_live_slot_injection_command() {
    const int before = potentiometerManager.getLastValue(2);
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_SLOT_VALUE,2,77"));
    const int after = potentiometerManager.getLastValue(2);
    TEST_ASSERT_NOT_EQUAL(before, after);
    TEST_ASSERT_EQUAL_INT(map(77, 0, 127, 0, 1023), after);
}

void test_dispatch_handles_profile_save_load_reset_commands() {
    g_activeProfile = 0;
    configManager.setActiveProfile(0);
    configManager.setPotChannel(0, 2);
    configManager.setPotCCNumber(0, 74);

    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SAVE_PROFILE,1"));
    TEST_ASSERT_EQUAL_UINT8(1, configManager.getActiveProfile());

    configManager.setPotChannel(0, 5);
    configManager.setPotCCNumber(0, 11);

    TEST_ASSERT_TRUE(testOnly_dispatchCommand("LOAD_PROFILE,1"));
    TEST_ASSERT_EQUAL_UINT8(1, configManager.getActiveProfile());
    TEST_ASSERT_EQUAL_UINT8(2, configManager.getPotChannel(0));
    TEST_ASSERT_EQUAL_UINT8(74, configManager.getPotCCNumber(0));

    TEST_ASSERT_TRUE(testOnly_dispatchCommand("RESET_PROFILE,2"));
    TEST_ASSERT_EQUAL_UINT8(2, configManager.getActiveProfile());
    TEST_ASSERT_EQUAL_UINT8(1, configManager.getPotChannel(0));
    TEST_ASSERT_EQUAL_UINT8(0, configManager.getPotCCNumber(0));
}

void test_dispatch_handles_macro_and_scene_snapshot_commands() {
    configManager.setPotChannel(0, 3);
    configManager.setPotCCNumber(0, 91);
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SAVE_MACRO_SLOT"));

    configManager.setPotChannel(0, 9);
    configManager.setPotCCNumber(0, 12);
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("RECALL_MACRO_SLOT"));
    TEST_ASSERT_EQUAL_UINT8(3, configManager.getPotChannel(0));
    TEST_ASSERT_EQUAL_UINT8(91, configManager.getPotCCNumber(0));

    TEST_ASSERT_TRUE(
        testOnly_dispatchCommand("{\"cmd\":\"SAVE_SCENE\",\"slot\":0,\"name\":\"Bench\"}"));
    configManager.setPotChannel(0, 6);
    configManager.setPotCCNumber(0, 21);
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("{\"cmd\":\"RECALL_SCENE\",\"slot\":0}"));
    TEST_ASSERT_EQUAL_UINT8(3, configManager.getPotChannel(0));
    TEST_ASSERT_EQUAL_UINT8(91, configManager.getPotCCNumber(0));
}

void test_dispatch_rejects_unknown_command() {
    webSerialStreaming = false;
    TEST_ASSERT_FALSE(testOnly_dispatchCommand("NONEXISTENT_COMMAND"));
    TEST_ASSERT_FALSE(webSerialStreaming);
}
