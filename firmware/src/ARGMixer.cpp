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

uint8_t computeArgLevel(const SlotARGConfig &config,
                        const std::array<uint8_t, NUM_ENVELOPES> &levels) {
    const SlotARGConfig cfg = sanitizeSlotArg(config);
    const int A = levels[cfg.sourceA];
    const int B = levels[cfg.sourceB];
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

// Compatibility adapter for callers that still own follower objects.
uint8_t computeSlotArgLevel(const MIDISlot &slot, const std::vector<EnvelopeFollower> &followers) {
    std::array<uint8_t, NUM_ENVELOPES> levels{};
    for (size_t i = 0; i < levels.size() && i < followers.size(); ++i) {
        levels[i] = static_cast<uint8_t>(constrain(followers[i].getEnvelopeLevel(), 0, 127));
    }
    if (!slot.arg.enabled) {
        const int followerIndex = slot.getEnvelopeFollowerIndex();
        return followerIndex >= 0 && followerIndex < static_cast<int>(levels.size())
                   ? levels[static_cast<size_t>(followerIndex)]
                   : 0;
    }
    return computeArgLevel(slot.arg, levels);
}
