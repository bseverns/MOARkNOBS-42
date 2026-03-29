// ARG (Arithmetic Reference Generator) mixing glues together pairs of envelope
// followers so a slot can riff on two modulation sources at once. This file is
// intentionally loud about how the math works so folks new to DSP can track
// the flow: sanitize the config, grab two envelope levels, then blend them with
// whichever punky operator you picked in the editor. No hidden magic, just
// pointers, references, and a handful of standard-library helpers earning
// their keep.

#include "ARGMixer.h"

#include <Arduino.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "EnvelopeFollower.h"
#include "Globals.h"

// Clamp and normalize one slot's ARG configuration before runtime math touches it.
SlotARGConfig sanitizeSlotArg(const SlotARGConfig &candidate) {
    SlotARGConfig sanitized = candidate;
    sanitized.enabled = candidate.enabled ? 1 : 0;

    // `method` comes in as a raw integer from EEPROM or JSON. Clamping it here
    // keeps us from indexing past the enum even if someone edits their config
    // by hand at 2 a.m. while hopped up on solder fumes.

    const uint8_t maxMethod = static_cast<uint8_t>(ARGMethod::XORR);
    if (static_cast<uint8_t>(sanitized.method) > maxMethod) {
        sanitized.method = ARGMethod::PLUS;
    }

    // Source indices are stored as raw bytes so the firmware can persist them
    // without worrying about pointer lifetimes. We bounce them back into range
    // before touching the envelope follower vector.
    if (sanitized.sourceA >= NUM_ENVELOPES) {
        sanitized.sourceA = 0;
    }
    if (sanitized.sourceB >= NUM_ENVELOPES) {
        sanitized.sourceB = (sanitized.sourceA + 1) % NUM_ENVELOPES;
    }
    if (sanitized.sourceA == sanitized.sourceB) {
        sanitized.sourceB = (sanitized.sourceA + 1) % NUM_ENVELOPES;
    }

    return sanitized;
}

// Compute one slot's effective ARG level from its chosen follower pair and operator.
uint8_t computeSlotArgLevel(const MIDISlot &slot, const std::vector<EnvelopeFollower> &followers) {
    const SlotARGConfig &cfg = slot.arg;
    auto fetchLevel = [&](uint8_t idx) -> int {
        if (idx < followers.size()) {
            return followers[idx].getEnvelopeLevel();
        }
        return 0;
    };

    // If ARG is disabled we fall back to the slot's linked follower. Think of
    // this as "bypass"—the same wiring but no extra maths.
    if (!cfg.enabled) {
        const auto followerIndex = static_cast<size_t>(slot.ef.followerIndex);
        if (slot.ef.followerIndex >= 0 && followerIndex < followers.size()) {
            return static_cast<uint8_t>(followers[followerIndex].getEnvelopeLevel());
        }
        return 0;
    }

    const int A = fetchLevel(cfg.sourceA);
    const int B = fetchLevel(cfg.sourceB);
    int result = 0;

    // Each operator is kept explicit so students can tinker without spelunking
    // into mysterious helper functions. Want to teach ratios, sums, xor? It's
    // all laid out right here.
    switch (cfg.method) {
    case ARGMethod::PLUS:
        result = A + B;
        break;
    case ARGMethod::MIN:
        result = A - B;
        break;
    case ARGMethod::PECK:
        result = B - A;
        break;
    case ARGMethod::SHAV:
        result = (A - B) / 10;
        break;
    case ARGMethod::SQAR:
        result = static_cast<int>(std::sqrt(static_cast<float>(A * A + B * B)));
        break;
    case ARGMethod::BABS:
        result = (B != 0) ? (A / std::abs(B)) : 0;
        break;
    case ARGMethod::TABS:
        result = (B != 0) ? ((10 * A) / std::abs(B)) : 0;
        break;
    case ARGMethod::MULT:
        result = (A * B) / 127;
        break;
    case ARGMethod::DIVI:
        result = (A * 127) / (B + 1);
        break;
    case ARGMethod::AVG:
        result = (A + B) / 2;
        break;
    case ARGMethod::XABS:
        result = std::abs(A - B);
        break;
    case ARGMethod::MAXX:
        result = std::max(A, B);
        break;
    case ARGMethod::MINN:
        result = std::min(A, B);
        break;
    case ARGMethod::XORR:
        result = A ^ B;
        break;
    }

    return static_cast<uint8_t>(constrain(result, 0, 127));
}
