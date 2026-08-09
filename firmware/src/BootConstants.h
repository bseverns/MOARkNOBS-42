#ifndef BOOT_CONSTANTS_H
#define BOOT_CONSTANTS_H

// Define the EEPROM address used for boot‑configuation requests.
// The exact value is not critical; it must be a valid EEPROM slot that
// isn’t otherwise used by the firm­ware. Using a low offset keeps the test simple.
constexpr uint8_t EEPROM_USB_CONFIG_BOOT_REQUEST = 0x12;

#endif // BOOT_CONSTANTS_H
