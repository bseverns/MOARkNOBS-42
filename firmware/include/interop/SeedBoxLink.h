#pragma once

#include <cstdint>

namespace seedbox {
namespace interop {
namespace mn42 {

class MIDIHandler; // forward declaration

class SeedBoxLink {
  public:
    static SeedBoxLink &instance();

    void begin(MIDIHandler *handler);
    void update();

    bool handleControlChange(uint8_t channel, uint8_t control, uint8_t value);
    void handleSysEx(const uint8_t *data, uint16_t length);

    bool hasAck() const { return _hasAck; }
    bool peerAlive() const;

  private:
    SeedBoxLink() = default;

    void sendHello();
    void sendAck();
    void sendKeepAlive();
    void sendIdentityPing();

    void markPeerPulse();

    MIDIHandler *_midi = nullptr;
    bool _hasAck = false;
    unsigned long _lastHelloMs = 0;
    unsigned long _lastKeepAliveMs = 0;
    unsigned long _lastPeerPulseMs = 0;
};

} // namespace mn42
} // namespace interop
} // namespace seedbox

