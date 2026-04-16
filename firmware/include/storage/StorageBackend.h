#ifndef STORAGE_BACKEND_H
#define STORAGE_BACKEND_H

#include <cstddef>
#include <cstdint>

class StorageBackend {
  public:
    virtual ~StorageBackend() = default;

    virtual uint16_t length() const = 0;
    virtual uint8_t read(int address) const = 0;
    virtual void update(int address, uint8_t value) = 0;
    virtual void readBytes(int address, void *dest, size_t len) const = 0;
    virtual void writeBytes(int address, const void *src, size_t len) = 0;
};

#endif // STORAGE_BACKEND_H
