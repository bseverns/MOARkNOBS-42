#pragma once

#include <cstdint>

namespace seedbox {
namespace interop {
namespace mn42 {

constexpr uint8_t kDefaultChannel = 1;

namespace cc {
constexpr uint8_t kHandshake = 14;
constexpr uint8_t kMode = 15;
constexpr uint8_t kSeedMorph = 16;
constexpr uint8_t kTransportGate = 17;
} // namespace cc

namespace handshake {
constexpr uint8_t kHello = 0x01;
constexpr uint8_t kAck = 0x11;
constexpr uint8_t kKeepAlive = 0x7F;

namespace product {
constexpr uint8_t kManufacturerId = 0x7D;
constexpr uint8_t kSignature0 = 0x4D; // 'M'
constexpr uint8_t kSignature1 = 0x4E; // 'N'
constexpr uint8_t kSignature2 = 0x42; // 'B'
constexpr uint8_t kPresenceFlag = 0x01;
} // namespace product

} // namespace handshake

namespace mode {
constexpr uint8_t kFollowExternalClock = 0x01;
constexpr uint8_t kExposeDebugMeters = 0x02;
constexpr uint8_t kArpAccent = 0x04;
constexpr uint8_t kLatchTransport = 0x08;
} // namespace mode

} // namespace mn42
} // namespace interop
} // namespace seedbox
