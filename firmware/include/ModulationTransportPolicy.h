#ifndef MODULATION_TRANSPORT_POLICY_H
#define MODULATION_TRANSPORT_POLICY_H

#include <array>
#include <cstddef>
#include <cstdint>

template <size_t SlotCount> class ModulationTransportPolicy {
  public:
    static constexpr uint16_t kInitialBytes = 15;
    static constexpr uint16_t kCapacityBytes = 64;
    static constexpr uint8_t kBytesPerMs = 3;

    struct Candidate {
        bool pending = false;
        bool note = false;
        uint8_t value = 0;
        uint8_t cost = 0;
    };

    void reset(uint32_t currentMs) {
        bytes_ = kInitialBytes;
        updatedAtMs_ = currentMs;
        cursor_ = 0;
    }

    void refill(uint32_t currentMs) {
        const uint32_t elapsedMs = currentMs - updatedAtMs_;
        if (elapsedMs == 0) return;
        if (elapsedMs >= (kCapacityBytes / kBytesPerMs) + 1U) {
            bytes_ = kCapacityBytes;
        } else {
            const uint32_t replenished = bytes_ + elapsedMs * kBytesPerMs;
            bytes_ = static_cast<uint16_t>(
                replenished < kCapacityBytes ? replenished : kCapacityBytes);
        }
        updatedAtMs_ = currentMs;
    }

    int nextCandidate(const std::array<Candidate, SlotCount> &candidates,
                      bool notesOnly) const {
        for (size_t offset = 0; offset < SlotCount; ++offset) {
            const size_t slotIndex = (cursor_ + offset) % SlotCount;
            if (!candidates[slotIndex].pending) continue;
            if (notesOnly && !candidates[slotIndex].note) continue;
            return static_cast<int>(slotIndex);
        }
        return -1;
    }

    bool canAdmit(uint8_t cost) const { return cost > 0 && cost <= bytes_; }

    void recordAttempt(size_t slotIndex) {
        cursor_ = static_cast<uint8_t>((slotIndex + 1U) % SlotCount);
    }

    bool recordSuccess(uint8_t cost) {
        if (!canAdmit(cost)) return false;
        bytes_ = static_cast<uint16_t>(bytes_ - cost);
        return true;
    }

    uint16_t availableBytes() const { return bytes_; }
    uint8_t cursor() const { return cursor_; }
    uint32_t updatedAtMs() const { return updatedAtMs_; }

  private:
    uint16_t bytes_ = kInitialBytes;
    uint32_t updatedAtMs_ = 0;
    uint8_t cursor_ = 0;
};

#endif // MODULATION_TRANSPORT_POLICY_H
