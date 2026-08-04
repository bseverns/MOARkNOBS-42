#ifndef LITTLEFS_STORAGE_BACKEND_H
#define LITTLEFS_STORAGE_BACKEND_H

#include "protocol/SceneStorage.h"
#include "storage/EepromStorageBackend.h"
#include "storage/StorageBackend.h"

#include <LittleFS.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

// A/B persistence backend. The data blobs are permanent; a pair of small,
// checksummed metadata records selects the committed blob. Interrupted writes
// can only damage the inactive blob or inactive metadata record.
class LittleFsStorageBackend final : public StorageBackend {
  public:
    static constexpr uint16_t kVirtualAddressSpace = 49152;
    static constexpr uint32_t kProgramDiskSize = 1024 * 1024;
    static_assert(kVirtualAddressSpace >= SceneStorage::kRequiredStorageBytes,
                  "LittleFS virtual storage cannot contain the declared scene layout");

    uint16_t length() const override { return kVirtualAddressSpace; }
    bool ready() const { return ensureReady(); }
    bool supportsTransactions() const override { return ensureReady(); }
    const char *statusDetail() const override { ensureReady(); return statusDetail_; }
    uint32_t generation() const override {
        MetaRecord meta{};
        return activeMeta(meta, nullptr) ? meta.generation : 0;
    }

    bool beginTransaction() override {
        if (!ensureReady() || transactionActive_) return false;
        MetaRecord meta{};
        if (!activeMeta(meta, nullptr)) return false;
        transactionBlob_ = static_cast<uint8_t>(1U - meta.activeBlob);
        if (transactionStorageClaimed_) return false;
        transactionStorageClaimed_ = true;
        transactionBuffer_ = transactionStorage_;
        File source = fs_.open(blobPath(meta.activeBlob), FILE_READ);
        if (!source || source.read(transactionBuffer_, kVirtualAddressSpace) != kVirtualAddressSpace) {
            if (source) source.close();
            releaseTransactionBuffer();
            return false;
        }
        source.close();
        transactionWriteFailed_ = false;
        transactionActive_ = true;
        return true;
    }

    bool commitTransaction() override {
        if (!transactionActive_) return false;
        File target = fs_.open(blobPath(transactionBlob_), FILE_WRITE);
        bool blobWritten = target && target.truncate(0) && target.seek(0, SeekSet);
        // LittleFS_Program does not guarantee that a single 48 KiB write is
        // accepted in one call. Stream the staged generation in bounded
        // blocks, just as migration/copy paths do.
        constexpr size_t kCommitChunkSize = 256;
        size_t written = 0;
        while (blobWritten && written < kVirtualAddressSpace) {
            const size_t count =
                std::min(kCommitChunkSize, static_cast<size_t>(kVirtualAddressSpace - written));
            blobWritten = target.write(transactionBuffer_ + written, count) == count;
            written += blobWritten ? count : 0;
        }
        if (target) {
            target.flush();
            target.close();
        }
        transactionActive_ = false;
        releaseTransactionBuffer();
        if (!blobWritten || written != kVirtualAddressSpace) {
            statusDetail_ = "commit_blob_write_failed";
            return false;
        }
        if (transactionWriteFailed_) {
            statusDetail_ = "commit_staged_write_failed";
            return false;
        }
        if (!fileHasExpectedSize(blobPath(transactionBlob_))) {
            statusDetail_ = "commit_blob_size_failed";
            return false;
        }

        MetaRecord current{};
        uint8_t currentSlot = 0;
        if (!activeMeta(current, &currentSlot)) {
            statusDetail_ = "commit_active_meta_failed";
            return false;
        }
        MetaRecord next{};
        next.flags = current.flags;
        next.generation = current.generation + 1;
        next.activeBlob = transactionBlob_;
        next.blobChecksum = fileChecksum(blobPath(transactionBlob_));
        if (!writeMetaSlot(static_cast<uint8_t>(1U - currentSlot), next)) {
            statusDetail_ = "commit_meta_write_failed";
            return false;
        }

        MetaRecord verified{};
        if (!readMetaSlot(static_cast<uint8_t>(1U - currentSlot), verified) ||
            verified.generation != next.generation || verified.activeBlob != next.activeBlob ||
            verified.blobChecksum != next.blobChecksum ||
            fileChecksum(blobPath(verified.activeBlob)) != verified.blobChecksum) {
            statusDetail_ = "commit_verify_failed";
            return false;
        }
        cacheActiveMeta(verified, static_cast<uint8_t>(1U - currentSlot));
        transactionWriteFailed_ = false;
        statusDetail_ = "ready";
        return true;
    }

    void abortTransaction() override {
        transactionActive_ = false;
        transactionWriteFailed_ = false;
        releaseTransactionBuffer();
    }

    uint8_t read(int address) const override {
        uint8_t value = 0xFF;
        readBytes(address, &value, sizeof(value));
        return value;
    }

    bool update(int address, uint8_t value) override {
        if (!ensureReady()) return fallback_.update(address, value);
        if (!contains(address)) return failWrite();
        if (read(address) == value) return true;
        return writeBytes(address, &value, sizeof(value));
    }

    void readBytes(int address, void *dest, size_t len) const override {
        if (!dest || !len) return;
        std::memset(dest, 0xFF, len);
        if (!ensureReady()) {
            fallback_.readBytes(address, dest, len);
            return;
        }
        if (!contains(address, len)) return;
        if (transactionActive_ && transactionBuffer_) {
            std::memcpy(dest, transactionBuffer_ + address, len);
            return;
        }
        File file = fs_.open(currentBlobPath(), FILE_READ);
        if (!file || !file.seek(static_cast<uint64_t>(address), SeekSet) ||
            file.read(static_cast<uint8_t *>(dest), len) != len) {
            if (file) file.close();
            return;
        }
        file.close();
    }

    bool writeBytes(int address, const void *src, size_t len) override {
        if (!src || !len) return failWrite();
        if (!ensureReady()) return fallback_.writeBytes(address, src, len);
        if (!contains(address, len)) return failWrite();
        if (transactionActive_ && transactionBuffer_) {
            std::memcpy(transactionBuffer_ + address, src, len);
            return true;
        }
        File file = fs_.open(currentBlobPath(), FILE_WRITE);
        if (!file || !file.seek(static_cast<uint64_t>(address), SeekSet)) {
            if (file) file.close();
            return failWrite();
        }
        const bool ok = file.write(static_cast<const uint8_t *>(src), len) == len;
        file.flush();
        file.close();
        return ok ? true : failWrite();
    }

  private:
    static constexpr const char *kBlobAPath = "/config_A.bin";
    static constexpr const char *kBlobBPath = "/config_B.bin";
    static constexpr const char *kMetaAPath = "/config.meta.A";
    static constexpr const char *kMetaBPath = "/config.meta.B";
    static constexpr const char *kLegacyBlobPath = "/config_storage.bin";
    static constexpr uint32_t kMetaMagic = 0x4D4B425A;
    static constexpr uint16_t kMetaVersion = 3;
    static constexpr uint16_t kMetaFlagMigratedFromEeprom = 0x0001;

    struct __attribute__((packed)) MetaRecord {
        uint32_t magic = kMetaMagic;
        uint16_t version = kMetaVersion;
        uint16_t flags = 0;
        uint32_t generation = 0;
        uint8_t activeBlob = 0;
        uint32_t blobChecksum = 0;
        uint32_t recordChecksum = 0;
    };

    static const char *blobPath(uint8_t index) { return index == 0 ? kBlobAPath : kBlobBPath; }
    static const char *metaPath(uint8_t index) { return index == 0 ? kMetaAPath : kMetaBPath; }
    bool failWrite() { if (transactionActive_) transactionWriteFailed_ = true; return false; }
    void releaseTransactionBuffer() const {
        if (transactionBuffer_ == transactionStorage_) transactionStorageClaimed_ = false;
        transactionBuffer_ = nullptr;
    }

    bool ensureReady() const {
        if (readyChecked_) return littlefsReady_;
        readyChecked_ = true;
        if (!ensureMounted()) {
            statusDetail_ = "mount_failed";
            littlefsReady_ = false;
            return false;
        }
        littlefsReady_ = ensureStorageFiles();
        if (littlefsReady_) statusDetail_ = "ready";
        return littlefsReady_;
    }

    bool ensureMounted() const {
        if (mountAttempted_) return mounted_;
        mountAttempted_ = true;
        mounted_ = fs_.begin(kProgramDiskSize);
        return mounted_;
    }

    bool ensureStorageFiles() const {
        if (!fileHasExpectedSize(kBlobAPath)) {
            if (!migrateLegacyBlob() && !createBlankBlob(kBlobAPath)) {
                statusDetail_ = "blob_a_create_failed";
                return false;
            }
        }
        bool hasData = false;
        if (!blobHasNonFFData(kBlobAPath, hasData)) {
            statusDetail_ = "blob_a_read_failed";
            return false;
        }
        if (!hasData && !migrateFromEeprom()) {
            statusDetail_ = "eeprom_migration_failed";
            return false;
        }
        if (!fileHasExpectedSize(kBlobBPath)) {
            // A failed/interrupted commit can leave the inactive file entry in
            // a shape that cannot be truncated reliably. It is not authority,
            // so recreate it from the intact active A generation.
            fs_.remove(kBlobBPath);
            if (!copyFile(kBlobAPath, kBlobBPath)) {
                statusDetail_ = "blob_b_copy_failed";
                return false;
            }
        }

        MetaRecord meta{};
        if (activeMeta(meta, nullptr)) return true;
        meta.blobChecksum = fileChecksum(kBlobAPath);
        if (!writeMetaSlot(0, meta)) {
            statusDetail_ = "metadata_repair_failed";
            return false;
        }
        cacheActiveMeta(meta, 0);
        return true;
    }

    bool migrateLegacyBlob() const {
        File legacy = fs_.open(kLegacyBlobPath, FILE_READ);
        if (!legacy) return false;
        File target = fs_.open(kBlobAPath, FILE_WRITE);
        if (!target) { legacy.close(); return false; }
        target.truncate(0);
        uint8_t chunk[128];
        size_t copied = 0;
        while (legacy.available() && copied < kVirtualAddressSpace) {
            const size_t wanted = std::min(sizeof(chunk), static_cast<size_t>(kVirtualAddressSpace - copied));
            const size_t got = legacy.read(chunk, wanted);
            if (got == 0 || target.write(chunk, got) != got) { legacy.close(); target.close(); return false; }
            copied += got;
        }
        legacy.close();
        const bool filled = fillToExpectedSize(target, copied);
        target.flush(); target.close();
        return filled;
    }

    bool createBlankBlob(const char *path) const {
        File file = fs_.open(path, FILE_WRITE);
        if (!file) return false;
        file.truncate(0);
        const bool ok = fillToExpectedSize(file, 0);
        file.flush(); file.close();
        return ok;
    }

    bool fillToExpectedSize(File &file, size_t size) const {
        uint8_t fill[128]; std::memset(fill, 0xFF, sizeof(fill));
        while (size < kVirtualAddressSpace) {
            const size_t count = std::min(sizeof(fill), static_cast<size_t>(kVirtualAddressSpace - size));
            if (file.write(fill, count) != count) return false;
            size += count;
        }
        return true;
    }

    bool migrateFromEeprom() const {
        File file = fs_.open(kBlobAPath, FILE_WRITE);
        if (!file || !file.truncate(0)) { if (file) file.close(); return false; }
        const uint16_t count = std::min<uint16_t>(fallback_.length(), kVirtualAddressSpace);
        uint8_t chunk[128];
        uint16_t copied = 0;
        while (copied < count) {
            const size_t size = std::min(sizeof(chunk), static_cast<size_t>(count - copied));
            for (size_t i = 0; i < size; ++i) chunk[i] = fallback_.read(copied + i);
            if (file.write(chunk, size) != size) { file.close(); return false; }
            copied = static_cast<uint16_t>(copied + size);
        }
        if (!fillToExpectedSize(file, copied)) { file.close(); return false; }
        file.flush(); file.close();
        return copyFile(kBlobAPath, kBlobBPath);
    }

    uint32_t fnvFile(const char *path) const {
        File file = fs_.open(path, FILE_READ);
        if (!file) return 0;
        uint32_t hash = 2166136261UL; uint8_t chunk[128];
        size_t remaining = kVirtualAddressSpace;
        while (remaining) {
            const size_t count = std::min(remaining, sizeof(chunk));
            if (file.read(chunk, count) != count) { file.close(); return 0; }
            for (size_t i = 0; i < count; ++i) { hash ^= chunk[i]; hash *= 16777619UL; }
            remaining -= count;
        }
        file.close(); return hash;
    }
    uint32_t fileChecksum(const char *path) const { return fileHasExpectedSize(path) ? fnvFile(path) : 0; }
    uint32_t metaChecksum(const MetaRecord &meta) const {
        uint32_t hash = 2166136261UL; const auto *bytes = reinterpret_cast<const uint8_t *>(&meta);
        for (size_t i = 0; i < offsetof(MetaRecord, recordChecksum); ++i) { hash ^= bytes[i]; hash *= 16777619UL; }
        return hash;
    }
    bool readMetaSlot(uint8_t slot, MetaRecord &meta) const {
        File file = fs_.open(metaPath(slot), FILE_READ);
        if (!file || file.size() != sizeof(meta) || file.read(reinterpret_cast<uint8_t *>(&meta), sizeof(meta)) != sizeof(meta)) { if (file) file.close(); return false; }
        file.close();
        return meta.magic == kMetaMagic && meta.version == kMetaVersion && meta.activeBlob < 2 &&
               meta.recordChecksum == metaChecksum(meta);
    }
    bool writeMetaSlot(uint8_t slot, MetaRecord meta) const {
        meta.recordChecksum = metaChecksum(meta);
        File file = fs_.open(metaPath(slot), FILE_WRITE);
        if (!file || !file.seek(0, SeekSet)) { if (file) file.close(); return false; }
        const bool ok = file.write(reinterpret_cast<const uint8_t *>(&meta), sizeof(meta)) == sizeof(meta);
        if (ok) file.truncate(sizeof(meta));
        file.flush(); file.close();
        return ok;
    }
    bool activeMeta(MetaRecord &out, uint8_t *slotOut) const {
        // Validating a metadata slot includes hashing its entire 48 KiB blob.
        // Configuration hydration performs many small reads, so repeating that
        // validation for every read can hold startup in flash scans for minutes.
        // The active blob cannot change behind this backend; validate once at
        // mount and refresh the cache only after a verified transaction commit.
        if (activeMetaCached_) {
            out = cachedActiveMeta_;
            if (slotOut) *slotOut = cachedActiveMetaSlot_;
            return true;
        }
        // Ordinary compatibility writes can update the active virtual EEPROM
        // outside a bulk transaction. LittleFS already checks file integrity;
        // requiring the old whole-blob digest here would invalidate otherwise
        // healthy metadata on the next boot. New inactive generations are
        // still checksum-verified before their metadata slot is activated.
        MetaRecord a{}, b{};
        const bool validA = readMetaSlot(0, a) && fileHasExpectedSize(blobPath(a.activeBlob));
        const bool validB = readMetaSlot(1, b) && fileHasExpectedSize(blobPath(b.activeBlob));
        if (!validA && !validB) return false;
        const bool useB = validB && (!validA || b.generation > a.generation);
        out = useB ? b : a;
        const uint8_t activeSlot = useB ? 1 : 0;
        cacheActiveMeta(out, activeSlot);
        if (slotOut) *slotOut = activeSlot;
        return true;
    }
    void cacheActiveMeta(const MetaRecord &meta, uint8_t slot) const {
        cachedActiveMeta_ = meta;
        cachedActiveMetaSlot_ = slot;
        activeMetaCached_ = true;
    }
    const char *currentBlobPath() const {
        if (transactionActive_) return blobPath(transactionBlob_);
        MetaRecord meta{}; return activeMeta(meta, nullptr) ? blobPath(meta.activeBlob) : kBlobAPath;
    }
    bool fileHasExpectedSize(const char *path) const {
        File file = fs_.open(path, FILE_READ); if (!file) return false;
        const bool ok = file.size() == kVirtualAddressSpace; file.close(); return ok;
    }
    bool blobHasNonFFData(const char *path, bool &hasData) const {
        hasData = false; File file = fs_.open(path, FILE_READ); if (!file) return false;
        uint8_t chunk[128]; while (file.available()) { const size_t n = file.read(chunk, sizeof(chunk)); for (size_t i = 0; i < n; ++i) if (chunk[i] != 0xFF) { hasData = true; file.close(); return true; } }
        file.close(); return true;
    }
    bool copyFile(const char *source, const char *destination) const {
        File in = fs_.open(source, FILE_READ); File out = fs_.open(destination, FILE_WRITE);
        if (!in || !out) { if (in) in.close(); if (out) out.close(); return false; }
        out.truncate(0); uint8_t chunk[128]; size_t remaining = kVirtualAddressSpace;
        while (remaining) { const size_t n = std::min(remaining, sizeof(chunk)); if (in.read(chunk, n) != n || out.write(chunk, n) != n) { in.close(); out.close(); return false; } remaining -= n; }
        out.flush(); in.close(); out.close(); return true;
    }

    mutable EepromStorageBackend fallback_;
    mutable LittleFS_Program fs_;
    mutable bool readyChecked_ = false, littlefsReady_ = false, mountAttempted_ = false, mounted_ = false;
    mutable const char *statusDetail_ = "not_checked";
    mutable bool activeMetaCached_ = false;
    mutable MetaRecord cachedActiveMeta_{};
    mutable uint8_t cachedActiveMetaSlot_ = 0;
    mutable bool transactionActive_ = false, transactionWriteFailed_ = false;
    mutable uint8_t *transactionBuffer_ = nullptr;
    mutable uint8_t transactionBlob_ = 0;
    // A bulk request remains resident while its JsonDocument is applied. Keep
    // the 48 KiB transaction image out of the scarce malloc/RAM1 pool.
    DMAMEM inline static uint8_t transactionStorage_[kVirtualAddressSpace] = {};
    inline static bool transactionStorageClaimed_ = false;
};

#endif
