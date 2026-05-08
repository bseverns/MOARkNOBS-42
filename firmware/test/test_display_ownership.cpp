#include "unity_config.h"
#include <unity.h>

#include "display/DisplayOwnership.h"

using display::Owner;
using display::OwnershipRequest;

void test_display_owner_od01_startup_dominates() {
    OwnershipRequest request{};
    request.startupActive = true;
    request.modalActive = true;
    request.statusActive = true;

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Owner::Startup),
                            static_cast<uint8_t>(display::resolveOwner(request)));
}

void test_display_owner_od02_config_modal_dominates_status() {
    OwnershipRequest request{};
    request.modalActive = true;
    request.statusActive = true;

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Owner::Modal),
                            static_cast<uint8_t>(display::resolveOwner(request)));
    TEST_ASSERT_FALSE(display::statusMayRender(request));
}

void test_display_owner_od03_lfo_modal_dominates_status() {
    OwnershipRequest request{};
    request.modalActive = true;
    request.statusActive = true;

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Owner::Modal),
                            static_cast<uint8_t>(display::resolveOwner(request)));
}

void test_display_owner_od04_jitter_modal_dominates_status() {
    OwnershipRequest request{};
    request.modalActive = true;
    request.statusActive = true;

    TEST_ASSERT_FALSE(display::statusMayRender(request));
}

void test_display_owner_od05_diagnostics_modal_dominates_status() {
    OwnershipRequest request{};
    request.modalActive = true;
    request.statusActive = true;

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Owner::Modal),
                            static_cast<uint8_t>(display::resolveOwner(request)));
}

void test_display_owner_od06_status_dominates_non_modal() {
    OwnershipRequest request{};
    request.statusActive = true;
    request.controlActive = true;
    request.screensaverDue = true;

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Owner::Status),
                            static_cast<uint8_t>(display::resolveOwner(request)));
}

void test_display_owner_od07_control_follows_status_timeout() {
    OwnershipRequest request{};
    request.controlActive = true;

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Owner::Control),
                            static_cast<uint8_t>(display::resolveOwner(request)));
}

void test_display_owner_od08_screensaver_only_when_idle() {
    OwnershipRequest blocked{};
    blocked.modalActive = true;
    blocked.screensaverDue = true;
    TEST_ASSERT_FALSE(display::screensaverMayRender(blocked));

    OwnershipRequest idle{};
    idle.screensaverDue = true;
    TEST_ASSERT_TRUE(display::screensaverMayRender(idle));
}
