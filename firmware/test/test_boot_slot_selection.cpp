#include "unity_config.h"
#include <unity.h>
#include <Arduino.h>
#include <EEPROM.h>
#include "BootMode.h"
#include "../src/BootConstants.h"

// The EEPROM simulator that unit tests provide has these symbols.
void test_select_boot_slot_usbconfig() {
    // Arrange – ensure EEPROM memory is valid and set the boot selector flag
    const uint32_t empty = 0xFFFFFFFF;
    if (EEPROM.length() >= EEPROM_USB_CONFIG_BOOT_REQUEST + sizeof(empty)) {
        EEPROM.put(EEPROM_USB_CONFIG_BOOT_REQUEST, empty);
    }

    // Write the magic value that triggers USB configurator boot.
    const uint32_t marker = 0x4D4E4346; // kUsbConfiguratorBootMagic
    EEPROM.put(EEPROM_USB_CONFIG_BOOT_REQUEST, marker);

    // Act – call mode selector
    BootMode result = selectBootMode();

    // Assert – should be UsbConfigurator
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BootMode::UsbConfigurator), static_cast<uint8_t>(result));
}
