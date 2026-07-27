#ifndef EEPROM_STORAGE_BACKEND_H
#define EEPROM_STORAGE_BACKEND_H

#include "storage/StorageBackend.h"

#include <EEPROM.h>
#include <cstring>

class EepromStorageBackend final : public StorageBackend {
  public:
    uint16_t length() const override { return EEPROM.length(); }

    uint8_t read(int address) const override {
        return contains(address) ? EEPROM.read(address) : 0xFF;
    }

    void update(int address, uint8_t value) override {
        if (contains(address)) {
            EEPROM.update(address, value);
        }
    }

    void readBytes(int address, void *dest, size_t len) const override {
        if (!dest || !contains(address, len)) {
            if (dest && len) {
                memset(dest, 0xFF, len);
            }
            return;
        }
        auto *out = static_cast<uint8_t *>(dest);
        for (size_t i = 0; i < len; ++i) {
            out[i] = EEPROM.read(address + static_cast<int>(i));
        }
    }

    void writeBytes(int address, const void *src, size_t len) override {
        if (!src || !contains(address, len)) {
            return;
        }
        const auto *in = static_cast<const uint8_t *>(src);
        for (size_t i = 0; i < len; ++i) {
            EEPROM.update(address + static_cast<int>(i), in[i]);
        }
    }
};

#endif // EEPROM_STORAGE_BACKEND_H
