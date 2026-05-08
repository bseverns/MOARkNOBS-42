#ifndef MN42_DISPLAY_OWNERSHIP_H
#define MN42_DISPLAY_OWNERSHIP_H

namespace display {

enum class Owner {
    Startup,
    Modal,
    Status,
    Control,
    Screensaver,
    Baseline,
};

struct OwnershipRequest {
    bool startupActive = false;
    bool modalActive = false;
    bool statusActive = false;
    bool controlActive = false;
    bool screensaverDue = false;
};

inline Owner resolveOwner(const OwnershipRequest &request) {
    if (request.startupActive) {
        return Owner::Startup;
    }
    if (request.modalActive) {
        return Owner::Modal;
    }
    if (request.statusActive) {
        return Owner::Status;
    }
    if (request.controlActive) {
        return Owner::Control;
    }
    if (request.screensaverDue) {
        return Owner::Screensaver;
    }
    return Owner::Baseline;
}

inline bool statusMayRender(const OwnershipRequest &request) {
    return resolveOwner(request) == Owner::Status;
}

inline bool screensaverMayRender(const OwnershipRequest &request) {
    return resolveOwner(request) == Owner::Screensaver;
}

} // namespace display

#endif // MN42_DISPLAY_OWNERSHIP_H
