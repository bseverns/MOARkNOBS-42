#include "BootMode.h"

#include <EEPROM.h>

#include "Globals.h"

namespace {
constexpr uint32_t kUsbConfiguratorBootMagic = 0x4D4E4346; // "MNCF"
}

bool requestUsbConfiguratorBoot() {
    if (EEPROM.length() < EEPROM_USB_CONFIG_BOOT_REQUEST + sizeof(kUsbConfiguratorBootMagic)) {
        return false;
    }
    EEPROM.put(EEPROM_USB_CONFIG_BOOT_REQUEST, kUsbConfiguratorBootMagic);
    return true;
}

void clearUsbConfiguratorBootRequest() {
    const uint32_t empty = 0xFFFFFFFF;
    if (EEPROM.length() < EEPROM_USB_CONFIG_BOOT_REQUEST + sizeof(empty)) {
        return;
    }
    EEPROM.put(EEPROM_USB_CONFIG_BOOT_REQUEST, empty);
}

BootMode selectBootMode() {
    if (EEPROM.length() < EEPROM_USB_CONFIG_BOOT_REQUEST + sizeof(kUsbConfiguratorBootMagic)) {
        return BootMode::StandaloneRuntime;
    }

    uint32_t marker = 0xFFFFFFFF;
    EEPROM.get(EEPROM_USB_CONFIG_BOOT_REQUEST, marker);
    if (marker == kUsbConfiguratorBootMagic) {
        clearUsbConfiguratorBootRequest();
        return BootMode::UsbConfigurator;
    }

    return BootMode::StandaloneRuntime;
}
