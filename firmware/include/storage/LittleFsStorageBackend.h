#ifndef LITTLEFS_STORAGE_BACKEND_H
#define LITTLEFS_STORAGE_BACKEND_H

#include "storage/EepromStorageBackend.h"
#include "storage/StorageBackend.h"

#include <LittleFS.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

class LittleFsStorageBackend final : public StorageBackend {
  public:
    static constexpr uint16_t kVirtualAddressSpace = 16384;
    static constexpr uint32_t kProgramDiskSize = 1024 * 1024;

    LittleFsStorageBackend() = default;

    uint16_t length() const override { return kVirtualAddressSpace; }

    bool ready() const { return ensureReady(); }
    bool supportsTransactions() const override { return ensureReady(); }
    uint32_t generation() const override {
        MetaRecord meta{};
        return readMetaRecord(meta) ? meta.generation : 0;
    }

    bool beginTransaction() override {
        if (!ensureReady() || transactionActive_) return false;
        if (!copyFile(kStorageBlobPath, kStagingBlobPath)) return false;
        transactionActive_ = true;
        return true;
    }

    bool commitTransaction() override {
        if (!transactionActive_) return false;
        transactionActive_ = false;
        if (!fileHasExpectedSize(kStagingBlobPath)) return false;
        fs_.remove(kPreviousBlobPath);
        // If power fails after the first rename, boot recovery restores the
        // previous complete generation.  If it fails after the second, the
        // new complete generation is already active.
        if (!fs_.rename(kStorageBlobPath, kPreviousBlobPath)) return false;
        if (!fs_.rename(kStagingBlobPath, kStorageBlobPath)) {
            fs_.rename(kPreviousBlobPath, kStorageBlobPath);
            return false;
        }
        MetaRecord meta{};
        if (!readMetaRecord(meta)) {
            meta = MetaRecord{};
        }
        ++meta.generation;
        if (!writeMetaRecord(meta)) {
            return false;
        }
        fs_.remove(kPreviousBlobPath);
        return true;
    }

    void abortTransaction() override {
        transactionActive_ = false;
        if (ensureMounted()) fs_.remove(kStagingBlobPath);
    }

    uint8_t read(int address) const override {
        if (!ensureReady()) {
            return fallback_.read(address);
        }
        uint8_t value = 0xFF;
        readBytes(address, &value, sizeof(value));
        return value;
    }

    void update(int address, uint8_t value) override {
        if (!ensureReady()) {
            fallback_.update(address, value);
            return;
        }
        if (!addressInRange(address)) {
            return;
        }
        const uint8_t current = read(address);
        if (current == value) {
            return;
        }
        writeBytes(address, &value, sizeof(value));
    }

    void readBytes(int address, void *dest, size_t len) const override {
        if (!dest || len == 0) {
            return;
        }

        auto *out = static_cast<uint8_t *>(dest);
        std::memset(out, 0xFF, len);

        if (!ensureReady()) {
            fallback_.readBytes(address, dest, len);
            return;
        }

        if (!contains(address, len)) {
            return;
        }

        File file = fs_.open(currentBlobPath(), FILE_READ);
        if (!file) {
            return;
        }
        if (!file.seek(static_cast<uint64_t>(address), SeekSet)) {
            file.close();
            return;
        }
        file.read(out, len);
        file.close();
    }

    void writeBytes(int address, const void *src, size_t len) override {
        if (!src || len == 0) {
            return;
        }

        if (!ensureReady()) {
            fallback_.writeBytes(address, src, len);
            return;
        }

        if (!contains(address, len)) {
            return;
        }

        const auto *in = static_cast<const uint8_t *>(src);

        File file = fs_.open(currentBlobPath(), FILE_WRITE);
        if (!file) {
            return;
        }
        if (!file.seek(static_cast<uint64_t>(address), SeekSet)) {
            file.close();
            return;
        }
        file.write(in, len);
        file.flush();
        file.close();
    }

  private:
    static constexpr const char *kStorageBlobPath = "/config_storage.bin";
    static constexpr const char *kStagingBlobPath = "/config_storage.staging.bin";
    static constexpr const char *kPreviousBlobPath = "/config_storage.previous.bin";
    static constexpr const char *kStorageMetaPath = "/config_storage.meta";
    static constexpr uint32_t kMetaMagic = 0x4D4B425A; // "MKBZ"
    static constexpr uint16_t kMetaVersion = 2;
    static constexpr uint16_t kMetaFlagMigratedFromEeprom = 0x0001;

    struct MetaRecord {
        uint32_t magic = kMetaMagic;
        uint16_t version = kMetaVersion;
        uint16_t flags = 0;
        uint32_t generation = 0;
    };

    bool addressInRange(int address) const {
        return address >= 0 && address < static_cast<int>(kVirtualAddressSpace);
    }

    bool ensureReady() const {
        if (readyChecked_) {
            return littlefsReady_;
        }

        readyChecked_ = true;

        if (!ensureMounted()) {
            littlefsReady_ = false;
            return false;
        }
        if (!ensureStorageFile()) {
            littlefsReady_ = false;
            return false;
        }
        if (!ensureMigrationMarker()) {
            littlefsReady_ = false;
            return false;
        }

        littlefsReady_ = true;
        return true;
    }

    bool ensureMounted() const {
        if (mountAttempted_) {
            return mounted_;
        }

        mountAttempted_ = true;
        mounted_ = fs_.begin(kProgramDiskSize);
        return mounted_;
    }

    bool ensureStorageFile() const {
        if (!ensureMounted()) {
            return false;
        }

        // Recover an interrupted generation switch before considering a new
        // transaction.  The previous file is never removed until the new one
        // has become the active complete blob.
        File active = fs_.open(kStorageBlobPath, FILE_READ);
        if (!active) {
            File previous = fs_.open(kPreviousBlobPath, FILE_READ);
            if (previous) {
                previous.close();
                fs_.rename(kPreviousBlobPath, kStorageBlobPath);
            }
        } else {
            active.close();
        }

        File existing = fs_.open(kStorageBlobPath, FILE_READ);
        if (existing) {
            const uint64_t size = existing.size();
            existing.close();
            if (size >= kVirtualAddressSpace) {
                return true;
            }
        }

        File file = fs_.open(kStorageBlobPath, FILE_WRITE);
        if (!file) {
            return false;
        }

        uint64_t size = file.size();
        if (size > kVirtualAddressSpace) {
            file.truncate(kVirtualAddressSpace);
            size = kVirtualAddressSpace;
        }

        if (!file.seek(size, SeekSet)) {
            file.close();
            return false;
        }

        uint8_t fill[64];
        std::memset(fill, 0xFF, sizeof(fill));

        while (size < kVirtualAddressSpace) {
            const size_t chunk = std::min<uint64_t>(
                sizeof(fill), static_cast<uint64_t>(kVirtualAddressSpace) - size);
            if (file.write(fill, chunk) != chunk) {
                file.close();
                return false;
            }
            size += chunk;
        }

        file.flush();
        file.close();
        return true;
    }

    bool ensureMigrationMarker() const {
        MetaRecord meta{};
        if (readMetaRecord(meta) && meta.magic == kMetaMagic && meta.version == kMetaVersion) {
            return true;
        }

        bool migratedFromEeprom = false;
        bool blobHasData = false;
        if (!blobHasNonFFData(blobHasData)) {
            return false;
        }
        if (!blobHasData) {
            if (!migrateFromEeprom()) {
                return false;
            }
            migratedFromEeprom = true;
        }

        MetaRecord updated{};
        if (migratedFromEeprom) {
            updated.flags |= kMetaFlagMigratedFromEeprom;
        }
        return writeMetaRecord(updated);
    }

    bool readMetaRecord(MetaRecord &meta) const {
        File file = fs_.open(kStorageMetaPath, FILE_READ);
        if (!file) {
            return false;
        }
        if (file.size() < sizeof(MetaRecord)) {
            file.close();
            return false;
        }
        const size_t readCount = file.read(reinterpret_cast<uint8_t *>(&meta), sizeof(MetaRecord));
        file.close();
        return readCount == sizeof(MetaRecord);
    }

    bool writeMetaRecord(const MetaRecord &meta) const {
        File file = fs_.open(kStorageMetaPath, FILE_WRITE);
        if (!file) {
            return false;
        }
        if (!file.seek(0, SeekSet)) {
            file.close();
            return false;
        }
        const size_t written =
            file.write(reinterpret_cast<const uint8_t *>(&meta), sizeof(MetaRecord));
        if (written != sizeof(MetaRecord)) {
            file.close();
            return false;
        }
        file.truncate(sizeof(MetaRecord));
        file.flush();
        file.close();
        return true;
    }

    bool blobHasNonFFData(bool &hasData) const {
        hasData = false;
        File file = fs_.open(kStorageBlobPath, FILE_READ);
        if (!file) {
            return false;
        }
        uint8_t chunk[64];
        while (file.available()) {
            const size_t readCount = file.read(chunk, sizeof(chunk));
            if (readCount == 0) {
                break;
            }
            for (size_t i = 0; i < readCount; ++i) {
                if (chunk[i] != 0xFF) {
                    hasData = true;
                    file.close();
                    return true;
                }
            }
        }
        file.close();
        return true;
    }

    bool migrateFromEeprom() const {
        File file = fs_.open(kStorageBlobPath, FILE_WRITE);
        if (!file) {
            return false;
        }
        if (!file.seek(0, SeekSet)) {
            file.close();
            return false;
        }

        const uint16_t bytesToCopy = std::min<uint16_t>(fallback_.length(), kVirtualAddressSpace);
        uint8_t chunk[64];
        uint16_t offset = 0;
        while (offset < bytesToCopy) {
            const uint16_t remaining = static_cast<uint16_t>(bytesToCopy - offset);
            const uint16_t chunkSize = std::min<uint16_t>(remaining, sizeof(chunk));
            for (uint16_t i = 0; i < chunkSize; ++i) {
                chunk[i] = fallback_.read(offset + i);
            }
            if (file.write(chunk, chunkSize) != chunkSize) {
                file.close();
                return false;
            }
            offset = static_cast<uint16_t>(offset + chunkSize);
        }

        file.flush();
        file.close();
        return true;
    }

    const char *currentBlobPath() const {
        return transactionActive_ ? kStagingBlobPath : kStorageBlobPath;
    }

    bool fileHasExpectedSize(const char *path) const {
        File file = fs_.open(path, FILE_READ);
        if (!file) return false;
        const bool valid = file.size() == kVirtualAddressSpace;
        file.close();
        return valid;
    }

    bool copyFile(const char *source, const char *destination) const {
        File in = fs_.open(source, FILE_READ);
        File out = fs_.open(destination, FILE_WRITE);
        if (!in || !out) {
            if (in) in.close();
            if (out) out.close();
            return false;
        }
        uint8_t chunk[128];
        size_t remaining = kVirtualAddressSpace;
        while (remaining > 0) {
            const size_t wanted = std::min(remaining, sizeof(chunk));
            if (in.read(chunk, wanted) != wanted || out.write(chunk, wanted) != wanted) {
                in.close(); out.close(); return false;
            }
            remaining -= wanted;
        }
        out.flush();
        in.close(); out.close();
        return true;
    }

    mutable EepromStorageBackend fallback_;
    mutable LittleFS_Program fs_;
    mutable bool readyChecked_ = false;
    mutable bool littlefsReady_ = false;
    mutable bool mountAttempted_ = false;
    mutable bool mounted_ = false;
    mutable bool transactionActive_ = false;
};

#endif // LITTLEFS_STORAGE_BACKEND_H
