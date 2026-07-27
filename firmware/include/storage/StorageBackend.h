#ifndef STORAGE_BACKEND_H
#define STORAGE_BACKEND_H

#include <cstddef>
#include <cstdint>

class StorageBackend {
  public:
    virtual ~StorageBackend() = default;

    virtual uint16_t length() const = 0;
    // All backends must reject a complete request that does not fit.  Callers
    // can use this before a multi-byte operation; adapters also enforce it.
    bool contains(int address, size_t len = 1) const {
        return address >= 0 && static_cast<size_t>(address) <= length() &&
               len <= static_cast<size_t>(length()) - static_cast<size_t>(address);
    }
    virtual uint8_t read(int address) const = 0;
    virtual void update(int address, uint8_t value) = 0;
    virtual void readBytes(int address, void *dest, size_t len) const = 0;
    virtual void writeBytes(int address, const void *src, size_t len) = 0;

    // Optional inactive-generation transaction support.  Hardware Apply uses
    // this rather than writing the active generation in place.
    virtual bool supportsTransactions() const { return false; }
    virtual bool beginTransaction() { return false; }
    virtual bool commitTransaction() { return false; }
    virtual void abortTransaction() {}
};

#endif // STORAGE_BACKEND_H
