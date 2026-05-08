#include "unity_config.h"
#include <unity.h>

#include "BootMode.h"
#include "BoardPowerProfile.h"
#include "ConfigManager.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "Log.h"
#include "Protocol.h"

void test_dispatch_handles_known_command() {
    webSerialStreaming = false;
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("HELLO"));
    TEST_ASSERT_TRUE(webSerialStreaming);
}

void test_dispatch_handles_documented_query_commands() {
    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("GET_MANIFEST"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"power_profile\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"led_brightness_cap\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"rail_topology_verified\""));

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("GET_SCHEMA"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"schema_version\""));

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("GET_CONFIG"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"led\""));
}

void test_dispatch_set_led_clamps_and_persists_board_cap() {
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_LED,255,1,2,3"));
    TEST_ASSERT_EQUAL_UINT8(BoardPowerProfile::kLedBrightnessCap, ledManager.getBrightness());

    uint8_t persisted = 0;
    CRGB color;
    configManager.loadLEDSettings(persisted, color);
    TEST_ASSERT_EQUAL_UINT8(BoardPowerProfile::kLedBrightnessCap, persisted);
    TEST_ASSERT_EQUAL_UINT8(1, color.r);
    TEST_ASSERT_EQUAL_UINT8(2, color.g);
    TEST_ASSERT_EQUAL_UINT8(3, color.b);
}

void test_dispatch_handles_enter_config_mode_command() {
    clearUsbConfiguratorBootRequest();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("ENTER_CONFIG_MODE"));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BootMode::UsbConfigurator),
                            static_cast<uint8_t>(selectBootMode()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BootMode::StandaloneRuntime),
                            static_cast<uint8_t>(selectBootMode()));
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

void test_dispatch_set_all_reports_negative_contract_errors() {
    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_ALL orphan"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"code\":\"orphan\""));

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_ALL {\"seq\":11,\"config\":{\"slots\":[]}}"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"code\":\"checksum\""));

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_ALL {\"seq\":12,\"checksum\":\"bad\","));
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_ALL bad}"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"code\":\"parse\""));
}
