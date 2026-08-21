#ifndef PROFILE_MIGRATION_H
#define PROFILE_MIGRATION_H

#include "MIDITypes.h"
#include "ProfileTypes.h"
#include "storage/StorageBackend.h"

#include <array>
#include <cstdint>

enum class ProfileDecodeResult : uint8_t {
    Success,
    InsufficientStorage,
    UnsupportedVersion,
    ChecksumMismatch,
};

// Decode and sanitize one current or legacy profile record without writing
// storage. Version 1 predates persisted follower indices, so it receives the
// live slot snapshot used by the historical loader behavior.
ProfileDecodeResult decodeStoredProfile(
    const StorageBackend &storage, uint16_t base,
    const std::array<MIDISlot, NUM_SLOTS> &liveSlots, ProfileData &profile);

#endif // PROFILE_MIGRATION_H
