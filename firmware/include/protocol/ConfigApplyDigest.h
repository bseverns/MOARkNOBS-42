#ifndef MN42_PROTOCOL_CONFIG_APPLY_DIGEST_H
#define MN42_PROTOCOL_CONFIG_APPLY_DIGEST_H

#include <Arduino.h>

namespace ConfigApplyDigest {
// Hash the normalized runtime and active persisted profile state after an
// atomic bulk apply. The result is stable across raw structure padding.
String computeAppliedStateChecksum();
} // namespace ConfigApplyDigest

#endif // MN42_PROTOCOL_CONFIG_APPLY_DIGEST_H
