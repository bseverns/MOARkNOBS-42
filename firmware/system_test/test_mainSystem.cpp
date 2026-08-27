#include <Arduino.h>
#include "SystemTestShim.h"

// This is the orchestral score for the "full system" PlatformIO environment.
// When you flash teensy40_full_system it runs every high-level integration
// test back-to-back so you can slam through a hardware regression in one go.

void test_long_press_detection();
void corrupt_primary_valid_backup();
void corrupted_primary_and_backup();
void test_eeprom_recovery_after_power_cycle();
void test_calibration_offsets_survive_power_cycle();
void test_high_index_envelope_assignment_survives_reload();
void test_brightness_and_color();
void test_update_interval_round_trip();
void test_filter_type_switching();
void test_random_mode_respects_jitter_depth();
void test_channel_and_cc();
void test_long_press_requires_confirm();
void test_double_press_ctrl2_cycles_midi_type();
void test_double_press_ctrl3_ctrl4_and_lfo_live_combo();
void test_ctrl5_fast_taps_update_tempo_without_lfo_toggle();
void test_ctrl3_single_waits_out_double_press_window();
void test_jitter_combo_updates_settings();
void test_diagnostics_blocks_jitter_mode_and_preserves_led_signature();
void test_config_mode_combo_autosaves_dirty_changes();
void test_clock_source_combo_toggles_follow_external();
void test_lfo_tuning_combo_and_route_cycle();

#if defined(FULL_SYSTEM_COMBINED)
SystemTestSummary runSystemTests() {
    UNITY_BEGIN();
    RUN_TEST(test_long_press_detection);
    RUN_TEST(test_long_press_requires_confirm);
    RUN_TEST(test_double_press_ctrl2_cycles_midi_type);
    RUN_TEST(test_double_press_ctrl3_ctrl4_and_lfo_live_combo);
    RUN_TEST(test_ctrl5_fast_taps_update_tempo_without_lfo_toggle);
    RUN_TEST(test_ctrl3_single_waits_out_double_press_window);
    RUN_TEST(test_jitter_combo_updates_settings);
    RUN_TEST(test_diagnostics_blocks_jitter_mode_and_preserves_led_signature);
    RUN_TEST(test_config_mode_combo_autosaves_dirty_changes);
    RUN_TEST(test_clock_source_combo_toggles_follow_external);
    RUN_TEST(test_lfo_tuning_combo_and_route_cycle);
    RUN_TEST(corrupt_primary_valid_backup);
    RUN_TEST(corrupted_primary_and_backup);
    RUN_TEST(test_eeprom_recovery_after_power_cycle);
    RUN_TEST(test_calibration_offsets_survive_power_cycle);
    RUN_TEST(test_high_index_envelope_assignment_survives_reload);
    RUN_TEST(test_brightness_and_color);
    RUN_TEST(test_update_interval_round_trip);
    RUN_TEST(test_filter_type_switching);
    RUN_TEST(test_random_mode_respects_jitter_depth);
    RUN_TEST(test_channel_and_cc);
    return UNITY_END();
}
#else
void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_long_press_detection);
    RUN_TEST(test_long_press_requires_confirm);
    RUN_TEST(test_double_press_ctrl2_cycles_midi_type);
    RUN_TEST(test_double_press_ctrl3_ctrl4_and_lfo_live_combo);
    RUN_TEST(test_ctrl5_fast_taps_update_tempo_without_lfo_toggle);
    RUN_TEST(test_ctrl3_single_waits_out_double_press_window);
    RUN_TEST(test_jitter_combo_updates_settings);
    RUN_TEST(test_diagnostics_blocks_jitter_mode_and_preserves_led_signature);
    RUN_TEST(test_config_mode_combo_autosaves_dirty_changes);
    RUN_TEST(test_clock_source_combo_toggles_follow_external);
    RUN_TEST(test_lfo_tuning_combo_and_route_cycle);
    RUN_TEST(corrupt_primary_valid_backup);
    RUN_TEST(corrupted_primary_and_backup);
    RUN_TEST(test_eeprom_recovery_after_power_cycle);
    RUN_TEST(test_calibration_offsets_survive_power_cycle);
    RUN_TEST(test_high_index_envelope_assignment_survives_reload);
    RUN_TEST(test_brightness_and_color);
    RUN_TEST(test_update_interval_round_trip);
    RUN_TEST(test_filter_type_switching);
    RUN_TEST(test_random_mode_respects_jitter_depth);
    RUN_TEST(test_channel_and_cc);
    UNITY_END();
}

void loop() {}
#endif
