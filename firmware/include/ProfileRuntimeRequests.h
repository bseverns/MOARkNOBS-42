#pragma once

#include <stdint.h>

// Explicit mailbox between UI/protocol producers and the main-loop profile
// lifecycle consumer. Reload and save remain independent so neither request
// can overwrite the other before the loop services them.
class ProfileRuntimeRequests {
  public:
    void requestReload() { pending_ |= kReload; }
    void requestSave() { pending_ |= kSave; }

    bool reloadPending() const { return (pending_ & kReload) != 0; }
    bool savePending() const { return (pending_ & kSave) != 0; }

    bool takeReload() { return take(kReload); }
    bool takeSave() { return take(kSave); }

    void clear() { pending_ = 0; }

  private:
    static constexpr uint8_t kReload = 1u << 0;
    static constexpr uint8_t kSave = 1u << 1;
    uint8_t pending_ = 0;

    bool take(uint8_t request) {
        const bool wasPending = (pending_ & request) != 0;
        pending_ &= static_cast<uint8_t>(~request);
        return wasPending;
    }
};
