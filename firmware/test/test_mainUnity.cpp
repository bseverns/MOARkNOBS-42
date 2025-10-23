#include "unity_config.h" // bundles Arduino and corrals usbMIDI into our stub
#include <unity.h>

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
void test_lowpass_highpass_response();
void test_system_report();
void test_scoped_analog_provider_nesting();
void test_sequence_provider_cycles_values();
void test_set_provider_returns_previous();
void test_digital_provider_overrides_matrix_reads();
void test_parse_basic_sysex_template();
void test_parse_14bit_sysex_template();
void test_parse_rejects_bad_template();
void test_bulk_config_assembler_handles_chunks();
void test_bulk_config_assembler_detects_overflow();
void test_format_ack_includes_checksum_and_seq();
void test_bulk_config_accepts_numeric_slot_type();
void test_bulk_config_accepts_wider_numeric_slot_type();
void test_bulk_config_accepts_integral_float_slot_type();
void test_bulk_config_accepts_type_name_alias();
#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
void test_program_change();
void test_aftertouch();
void test_mod_wheel();
void test_pitch_bend();
void test_send_nrpn();
void test_receive_nrpn();
void test_send_sysex();
void test_drop_unsupported_usb_type();
void test_usb_clock_tick_advances_counter();
void test_generate_clock_tick_advances_counter();
void test_handle_sysex_drops_oversize();
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
    RUN_TEST(test_bulk_config_assembler_handles_chunks);
    RUN_TEST(test_bulk_config_assembler_detects_overflow);
    RUN_TEST(test_format_ack_includes_checksum_and_seq);
    RUN_TEST(test_bulk_config_accepts_numeric_slot_type);
    RUN_TEST(test_bulk_config_accepts_wider_numeric_slot_type);
    RUN_TEST(test_bulk_config_accepts_integral_float_slot_type);
    RUN_TEST(test_bulk_config_accepts_type_name_alias);
    RUN_TEST(test_start_stop_cycle);
    RUN_TEST(test_pot_root_drives_default);
    RUN_TEST(test_slot_root_wins_over_pot);
    RUN_TEST(test_external_callback_sets_root);
    RUN_TEST(test_external_base_note_without_callback);
    RUN_TEST(test_external_missing_inputs_falls_back_to_pot);
    RUN_TEST(test_catches_up_when_ticks_pile_up);
    RUN_TEST(test_lowpass_highpass_response);
    RUN_TEST(test_system_report);
#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
    RUN_TEST(test_program_change);
    RUN_TEST(test_aftertouch);
    RUN_TEST(test_mod_wheel);
    RUN_TEST(test_pitch_bend);
    RUN_TEST(test_send_nrpn);
    RUN_TEST(test_receive_nrpn);
    RUN_TEST(test_send_sysex);
    RUN_TEST(test_drop_unsupported_usb_type);
    RUN_TEST(test_handle_sysex_drops_oversize);
    RUN_TEST(test_usb_clock_tick_advances_counter);
    RUN_TEST(test_generate_clock_tick_advances_counter);
#endif
    UNITY_END();
}

void loop() {}
