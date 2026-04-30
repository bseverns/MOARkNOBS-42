#include "unity_config.h" // bundles Arduino and corrals usbMIDI into our stub
#include <unity.h>

#include "Hardware/IO.h"

// Unity's Arduino harness insists on a sketch-style entry point.  This file is
// the conductor that runs every firmware unit test when you flash the
// teensy40_unity target.  Keep the RUN_TEST order roughly grouped so failures
// point you at the right subsystem.

void test_start_stop_cycle();
void test_pot_root_drives_default();
void test_slot_root_wins_over_pot();
void test_external_callback_sets_root();
void test_external_base_note_without_callback();
void test_external_missing_inputs_falls_back_to_pot();
void test_catches_up_when_ticks_pile_up();
void test_random_shape_respects_jitter_depth();
void test_updown_shape_walks_full_range();
void test_drunk_shape_is_deterministic();
void test_euclidean_lite_skips_steps();
void test_swing_delays_offbeat_notes();
void test_tempo_change_updates_tick_ms();
void test_profile_crc_rejects_corruption();
void test_profile_bounds_clamp();
void test_profile_round_trip_preserves_profile_payload();
void test_lowpass_highpass_response();
void test_system_report();
void test_dispatch_handles_known_command();
void test_dispatch_handles_enter_config_mode_command();
void test_dispatch_handles_live_slot_injection_command();
void test_dispatch_handles_profile_save_load_reset_commands();
void test_dispatch_handles_macro_and_scene_snapshot_commands();
void test_dispatch_rejects_unknown_command();
void test_profile_storage_commands_restore_and_reset_live_state();
void test_macro_and_scene_storage_commands_report_inventory_and_restore_state();
void test_scoped_analog_provider_nesting();
void test_sequence_provider_cycles_values();
void test_set_provider_returns_previous();
void test_digital_provider_overrides_matrix_reads();
void test_parse_basic_sysex_template();
void test_parse_14bit_sysex_template();
void test_parse_rejects_bad_template();
void test_sysex_template_config_mutation_stays_valid();
void test_bulk_config_assembler_handles_chunks();
void test_bulk_config_assembler_resyncs_on_wrapper_start();
void test_bulk_config_assembler_detects_overflow();
void test_format_ack_includes_checksum_and_seq();
void test_device_schema_advertises_runtime_controls();
void test_bulk_config_accepts_numeric_slot_type();
void test_bulk_config_accepts_wider_numeric_slot_type();
void test_bulk_config_accepts_integral_float_slot_type();
void test_bulk_config_accepts_type_name_alias();
void test_exponential_moving_average_clamps_alpha_bounds();
void test_exponential_moving_average_rounds_weighted_result();
void test_arg_sanitize_clamps_sources();
void test_compute_slot_arg_level_blends_followers();
void test_legacy_arg_migration_populates_slots();
void test_led_animator_cycles_mode_order();
void test_led_animator_invalid_mode_falls_back_to_static();
void test_command_queue_drops_oldest_on_overflow();
void test_command_queue_ingests_newline_terminated_input();
void test_command_queue_ignores_carriage_returns();
void test_command_queue_flushes_when_buffer_limit_is_hit();
void test_scheduler_initializes_recurring_task_layout();
void test_runtime_pending_note_off_fires_when_due();
void test_runtime_pending_note_off_overflow_tracks_drop();
void test_runtime_diagnostics_log_only_on_counter_changes();
void test_webserial_state_snapshot_emits_expected_json();
void test_webserial_slot_patch_emits_schema_and_legacy_payloads();
void test_ui_update_note_dynamics_maps_control_pots();
void test_ui_update_arp_tuning_updates_length_and_shape();
void test_ui_update_arp_tuning_edit_mode_updates_gate_and_octave();
void test_ui_update_filter_tuning_persists_slot_payload();
void test_ui_stream_webserial_state_uses_active_slot_context();
void test_wait_guard_clamps_when_threshold_exceeds_wait();
void test_wait_guard_preserves_positive_delta();
void test_waveform_ranges();
void test_phase_continuity_free_run();
void test_sync_ticks_at_120_bpm();
void test_div_mult_math();
void test_sample_hold_length();
void test_golden_stats();
void test_lfo_clock_consumes_ticks();
void test_peak_mode_rises_and_falls();
void test_rms_mode_converges();
void test_gate_mode_hysteresis();
void test_auto_baseline_converges();
void test_auto_gain_targets_level();
void test_stats_report_mode_and_value();
void test_envelope_stats_sine_trace();
void test_envelope_stats_step_trace();
void test_envelope_stats_idle_noise();
void test_no_heap_growth_over_fake_runtime();
#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
void test_program_change();
void test_aftertouch();
void test_mod_wheel();
void test_pitch_bend();
void test_send_nrpn();
void test_receive_nrpn();
void test_send_sysex();
void test_pot_burst_keeps_cc_counters_honest();
void test_long_sysex_payload_round_trips();
void test_drop_unsupported_usb_type();
void test_usb_clock_tick_advances_counter();
void test_generate_clock_tick_advances_counter();
void test_handle_sysex_drops_oversize();
void test_config_manager_wipes_legacy_slot_stride();
void test_config_mutation_during_stream_stays_valid();
void test_seedbox_link_begin_sends_hello_and_identity_ping();
void test_seedbox_link_hello_triggers_ack();
void test_seedbox_link_keepalive_and_timeout_restart_handshake();
void test_clock_start_stop_flags();
void test_clock_tick_stream_counts_cleanly();
void test_clock_ppqn_start_stop_continue();
void test_clock_ppqn_timing_accuracy_with_tempo_jump();
#endif

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_scoped_analog_provider_nesting);
    RUN_TEST(test_sequence_provider_cycles_values);
    RUN_TEST(test_set_provider_returns_previous);
    RUN_TEST(test_digital_provider_overrides_matrix_reads);
    RUN_TEST(test_parse_basic_sysex_template);
    RUN_TEST(test_parse_14bit_sysex_template);
    RUN_TEST(test_parse_rejects_bad_template);
    RUN_TEST(test_sysex_template_config_mutation_stays_valid);
    RUN_TEST(test_bulk_config_assembler_handles_chunks);
    RUN_TEST(test_bulk_config_assembler_resyncs_on_wrapper_start);
    RUN_TEST(test_bulk_config_assembler_detects_overflow);
    RUN_TEST(test_format_ack_includes_checksum_and_seq);
    RUN_TEST(test_device_schema_advertises_runtime_controls);
    RUN_TEST(test_bulk_config_accepts_numeric_slot_type);
    RUN_TEST(test_bulk_config_accepts_wider_numeric_slot_type);
    RUN_TEST(test_bulk_config_accepts_integral_float_slot_type);
    RUN_TEST(test_bulk_config_accepts_type_name_alias);
    RUN_TEST(test_exponential_moving_average_clamps_alpha_bounds);
    RUN_TEST(test_exponential_moving_average_rounds_weighted_result);
    RUN_TEST(test_arg_sanitize_clamps_sources);
    RUN_TEST(test_compute_slot_arg_level_blends_followers);
    RUN_TEST(test_legacy_arg_migration_populates_slots);
    RUN_TEST(test_led_animator_cycles_mode_order);
    RUN_TEST(test_led_animator_invalid_mode_falls_back_to_static);
    RUN_TEST(test_command_queue_drops_oldest_on_overflow);
    RUN_TEST(test_command_queue_ingests_newline_terminated_input);
    RUN_TEST(test_command_queue_ignores_carriage_returns);
    RUN_TEST(test_command_queue_flushes_when_buffer_limit_is_hit);
    RUN_TEST(test_scheduler_initializes_recurring_task_layout);
    RUN_TEST(test_runtime_pending_note_off_fires_when_due);
    RUN_TEST(test_runtime_pending_note_off_overflow_tracks_drop);
    RUN_TEST(test_runtime_diagnostics_log_only_on_counter_changes);
    RUN_TEST(test_webserial_state_snapshot_emits_expected_json);
    RUN_TEST(test_webserial_slot_patch_emits_schema_and_legacy_payloads);
    RUN_TEST(test_ui_update_note_dynamics_maps_control_pots);
    RUN_TEST(test_ui_update_arp_tuning_updates_length_and_shape);
    RUN_TEST(test_ui_update_arp_tuning_edit_mode_updates_gate_and_octave);
    RUN_TEST(test_ui_update_filter_tuning_persists_slot_payload);
    RUN_TEST(test_ui_stream_webserial_state_uses_active_slot_context);
    RUN_TEST(test_wait_guard_clamps_when_threshold_exceeds_wait);
    RUN_TEST(test_wait_guard_preserves_positive_delta);
    RUN_TEST(test_waveform_ranges);
    RUN_TEST(test_phase_continuity_free_run);
    RUN_TEST(test_sync_ticks_at_120_bpm);
    RUN_TEST(test_div_mult_math);
    RUN_TEST(test_sample_hold_length);
    RUN_TEST(test_golden_stats);
    RUN_TEST(test_lfo_clock_consumes_ticks);
    RUN_TEST(test_peak_mode_rises_and_falls);
    RUN_TEST(test_rms_mode_converges);
    RUN_TEST(test_gate_mode_hysteresis);
    RUN_TEST(test_auto_baseline_converges);
    RUN_TEST(test_auto_gain_targets_level);
    RUN_TEST(test_stats_report_mode_and_value);
    RUN_TEST(test_envelope_stats_sine_trace);
    RUN_TEST(test_envelope_stats_step_trace);
    RUN_TEST(test_envelope_stats_idle_noise);
    RUN_TEST(test_no_heap_growth_over_fake_runtime);
    RUN_TEST(test_start_stop_cycle);
    RUN_TEST(test_pot_root_drives_default);
    RUN_TEST(test_slot_root_wins_over_pot);
    RUN_TEST(test_external_callback_sets_root);
    RUN_TEST(test_external_base_note_without_callback);
    RUN_TEST(test_external_missing_inputs_falls_back_to_pot);
    RUN_TEST(test_catches_up_when_ticks_pile_up);
    RUN_TEST(test_random_shape_respects_jitter_depth);
    RUN_TEST(test_updown_shape_walks_full_range);
    RUN_TEST(test_drunk_shape_is_deterministic);
    RUN_TEST(test_euclidean_lite_skips_steps);
    RUN_TEST(test_swing_delays_offbeat_notes);
    RUN_TEST(test_tempo_change_updates_tick_ms);
    RUN_TEST(test_profile_crc_rejects_corruption);
    RUN_TEST(test_profile_bounds_clamp);
    RUN_TEST(test_profile_round_trip_preserves_profile_payload);
    RUN_TEST(test_lowpass_highpass_response);
    RUN_TEST(test_system_report);
    RUN_TEST(test_dispatch_handles_known_command);
    RUN_TEST(test_dispatch_handles_enter_config_mode_command);
    RUN_TEST(test_dispatch_handles_live_slot_injection_command);
    RUN_TEST(test_dispatch_handles_profile_save_load_reset_commands);
    RUN_TEST(test_dispatch_handles_macro_and_scene_snapshot_commands);
    RUN_TEST(test_dispatch_rejects_unknown_command);
    RUN_TEST(test_profile_storage_commands_restore_and_reset_live_state);
    RUN_TEST(test_macro_and_scene_storage_commands_report_inventory_and_restore_state);
#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
    RUN_TEST(test_program_change);
    RUN_TEST(test_aftertouch);
    RUN_TEST(test_mod_wheel);
    RUN_TEST(test_pot_burst_keeps_cc_counters_honest);
    RUN_TEST(test_pitch_bend);
    RUN_TEST(test_send_nrpn);
    RUN_TEST(test_receive_nrpn);
    RUN_TEST(test_send_sysex);
    RUN_TEST(test_long_sysex_payload_round_trips);
    RUN_TEST(test_drop_unsupported_usb_type);
    RUN_TEST(test_handle_sysex_drops_oversize);
    RUN_TEST(test_usb_clock_tick_advances_counter);
    RUN_TEST(test_generate_clock_tick_advances_counter);
    RUN_TEST(test_clock_start_stop_flags);
    RUN_TEST(test_clock_tick_stream_counts_cleanly);
    RUN_TEST(test_clock_ppqn_start_stop_continue);
    RUN_TEST(test_clock_ppqn_timing_accuracy_with_tempo_jump);
    RUN_TEST(test_config_manager_wipes_legacy_slot_stride);
    RUN_TEST(test_config_mutation_during_stream_stays_valid);
    RUN_TEST(test_seedbox_link_begin_sends_hello_and_identity_ping);
    RUN_TEST(test_seedbox_link_hello_triggers_ack);
    RUN_TEST(test_seedbox_link_keepalive_and_timeout_restart_handshake);
#endif
    UNITY_END();
}

void loop() {}

// Unity's per-test hooks keep IO providers from bleeding between test files.
void setUp() {
    hardware::resetAnalogReadProvider();
    hardware::resetDigitalReadProvider();
}

void tearDown() {
    hardware::resetAnalogReadProvider();
    hardware::resetDigitalReadProvider();
}
