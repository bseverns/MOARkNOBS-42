#include <Arduino.h>
#include <unity.h>

void test_start_stop_cycle();
void test_lowpass_highpass_response();
void test_long_press_detection();
void corrupt_primary_valid_backup();
void corrupted_primary_and_backup();
void test_eeprom_recovery_after_power_cycle();
void test_calibration_offsets_survive_power_cycle();
void test_brightness_and_color();
void test_update_interval_round_trip();
#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
void test_program_change();
void test_aftertouch();
void test_pitch_bend();
void test_send_nrpn();
void test_receive_nrpn();
void test_send_sysex();
void test_drop_unsupported_usb_type();
#endif
void test_filter_type_switching();
void test_channel_and_cc();

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_start_stop_cycle);
    RUN_TEST(test_lowpass_highpass_response);
    RUN_TEST(test_long_press_detection);
    RUN_TEST(corrupt_primary_valid_backup);
    RUN_TEST(corrupted_primary_and_backup);
    RUN_TEST(test_eeprom_recovery_after_power_cycle);
    RUN_TEST(test_calibration_offsets_survive_power_cycle);
    RUN_TEST(test_brightness_and_color);
    RUN_TEST(test_update_interval_round_trip);
#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
    RUN_TEST(test_program_change);
    RUN_TEST(test_aftertouch);
    RUN_TEST(test_pitch_bend);
    RUN_TEST(test_send_nrpn);
    RUN_TEST(test_receive_nrpn);
    RUN_TEST(test_send_sysex);
    RUN_TEST(test_drop_unsupported_usb_type);
#endif
    RUN_TEST(test_filter_type_switching);
    RUN_TEST(test_channel_and_cc);
    UNITY_END();
}

void loop() {}

