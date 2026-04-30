#ifndef BOOT_MODE_H
#define BOOT_MODE_H

#include <cstdint>

enum class BootMode : uint8_t { StandaloneRuntime, UsbConfigurator };

BootMode selectBootMode();
bool requestUsbConfiguratorBoot();
void clearUsbConfiguratorBootRequest();

#endif // BOOT_MODE_H
