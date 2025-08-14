#include "unity_config.h"  // bundles Arduino and corrals usbMIDI into our stub
#include <unity.h>

void test_start_stop_cycle();
void test_lowpass_highpass_response();
void test_system_report();
#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
void test_program_change();
void test_aftertouch();
void test_pitch_bend();
void test_send_nrpn();
void test_receive_nrpn();
void test_send_sysex();
void test_drop_unsupported_usb_type();
#endif

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_start_stop_cycle);
    RUN_TEST(test_lowpass_highpass_response);
    RUN_TEST(test_system_report);
#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
    RUN_TEST(test_program_change);
    RUN_TEST(test_aftertouch);
    RUN_TEST(test_pitch_bend);
    RUN_TEST(test_send_nrpn);
    RUN_TEST(test_receive_nrpn);
    RUN_TEST(test_send_sysex);
    RUN_TEST(test_drop_unsupported_usb_type);
#endif
    UNITY_END();
}

void loop() {}

