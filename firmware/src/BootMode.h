#ifndef BOOTMODE_H
#define BOOTMODE_H

#include <cstdint>

// Forward declaration of boot mode enum.
enum class BootMode : uint8_t {
    StandaloneRuntime = 0,
    UsbConfigurator   = 1
};

// Function prototypes implemented in BootMode.cpp.
bool requestUsbConfiguratorBoot();
void clearUsbConfiguratorBootRequest();
BootMode selectBootMode();

#endif // BOOTMODE_H
