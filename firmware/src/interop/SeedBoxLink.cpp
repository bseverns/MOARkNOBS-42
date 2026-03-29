#include "interop/SeedBoxLink.h"

#include "Log.h"
#include "MIDIHandler.h"
#include "TimeUtils.h"
#include "interop/mn42_map.h"
#include <array>

namespace seedbox {
namespace interop {
namespace mn42 {

namespace {
constexpr unsigned long kHelloRetryMs = 2000UL;
constexpr unsigned long kKeepAlivePeriodMs = 3000UL;
constexpr unsigned long kPeerTimeoutMs = 8000UL;
constexpr size_t kIdentityPacketLength = 7;
} // namespace

// Global singleton used by the runtime and MIDI handler to coordinate the SeedBox link.
SeedBoxLink &SeedBoxLink::instance() {
    static SeedBoxLink link;
    return link;
}

// Start the handshake state machine and immediately announce MN42 presence.
void SeedBoxLink::begin(::MIDIHandler *handler) {
    _midi = handler;
    _hasAck = false;
    _lastHelloMs = 0;
    _lastKeepAliveMs = 0;
    _lastPeerPulseMs = 0;
    sendHello();
    sendIdentityPing();
}

// Retry hello, send keepalive pulses, and time out the peer when it goes silent.
void SeedBoxLink::update() {
    if (!_midi)
        return;

    unsigned long nowMs = now();

    if (!_hasAck) {
        if (nowMs - _lastHelloMs >= kHelloRetryMs) {
            sendHello();
        }
        return;
    }

    if (nowMs - _lastKeepAliveMs >= kKeepAlivePeriodMs) {
        sendKeepAlive();
    }

    if (_hasAck && (nowMs - _lastPeerPulseMs) > kPeerTimeoutMs) {
        _hasAck = false;
        LOG_PRINTLN("SeedBox link timed out; waiting for new hello");
        sendHello();
    }
}

// Consume SeedBox handshake CC messages and keep the peer-alive timers up to date.
bool SeedBoxLink::handleControlChange(uint8_t channel, uint8_t control, uint8_t value) {
    if (channel != kDefaultChannel || control != cc::kHandshake)
        return false;

    switch (value) {
    case handshake::kHello:
        LOG_PRINTLN("SeedBox hello heard; answering ack");
        markPeerPulse();
        sendAck();
        break;
    case handshake::kAck:
        if (!_hasAck) {
            LOG_PRINTLN("SeedBox ack received");
        }
        _hasAck = true;
        markPeerPulse();
        break;
    case handshake::kKeepAlive:
        markPeerPulse();
        break;
    default:
        break;
    }
    return true;
}

// Treat a recognized identity SysEx packet as proof that the peer is still alive.
void SeedBoxLink::handleSysEx(const uint8_t *data, uint16_t length) {
    if (!data || length < kIdentityPacketLength)
        return;
    const size_t lastIndex = static_cast<size_t>(length) - 1;
    if (data[0] != 0xF0 || data[lastIndex] != 0xF7)
        return;
    if (data[1] != handshake::product::kManufacturerId)
        return;
    if (data[2] != handshake::product::kSignature0 || data[3] != handshake::product::kSignature1 ||
        data[4] != handshake::product::kSignature2)
        return;

    markPeerPulse();
}

// Report whether the peer is still considered alive inside the timeout window.
bool SeedBoxLink::peerAlive() const {
    if (!_hasAck)
        return false;
    return (now() - _lastPeerPulseMs) <= kPeerTimeoutMs;
}

// Broadcast the MN42 hello CC used to start or restart the session.
void SeedBoxLink::sendHello() {
    if (!_midi)
        return;
    _midi->sendControlChange(cc::kHandshake, handshake::kHello, kDefaultChannel);
    _lastHelloMs = now();
}

// Answer a SeedBox hello with the expected ack CC.
void SeedBoxLink::sendAck() {
    if (!_midi)
        return;
    _midi->sendControlChange(cc::kHandshake, handshake::kAck, kDefaultChannel);
}

// Send the periodic keepalive pulse once the session is established.
void SeedBoxLink::sendKeepAlive() {
    if (!_midi)
        return;
    _midi->sendControlChange(cc::kHandshake, handshake::kKeepAlive, kDefaultChannel);
    _lastKeepAliveMs = now();
}

// Send the MN42 identity SysEx burst so SeedBox can distinguish this rig from random controllers.
void SeedBoxLink::sendIdentityPing() {
    if (!_midi)
        return;
    constexpr std::array<uint8_t, kIdentityPacketLength> packet = {
        0xF0,
        handshake::product::kManufacturerId,
        handshake::product::kSignature0,
        handshake::product::kSignature1,
        handshake::product::kSignature2,
        handshake::product::kPresenceFlag,
        0xF7};
    _midi->sendSysEx(packet.data(), packet.size());
}

// Record the last time we heard from the peer.
void SeedBoxLink::markPeerPulse() { _lastPeerPulseMs = now(); }

} // namespace mn42
} // namespace interop
} // namespace seedbox
