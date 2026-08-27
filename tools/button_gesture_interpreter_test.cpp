#include "ButtonGestureInterpreter.h"

#include <algorithm>
#include <cassert>
#include <array>
#include <cstdint>

namespace {
bool hasEvent(const ButtonGestureEvents &events, ButtonGestureEventType type,
              uint8_t value) {
    for (uint8_t index = 0; index < events.count; ++index) {
        if (events.items[index].type == type && events.items[index].value == value) return true;
    }
    return false;
}

struct EventCounts {
    uint16_t comboPress = 0;
    uint16_t comboRelease = 0;
    uint16_t singlePress = 0;
    uint16_t doublePress = 0;

    void add(const ButtonGestureEvents &events, uint8_t expectedMask) {
        for (uint8_t index = 0; index < events.count; ++index) {
            const ButtonGestureEvent &event = events.items[index];
            if (event.type == ButtonGestureEventType::ComboPress) {
                assert(event.value == expectedMask);
                ++comboPress;
            } else if (event.type == ButtonGestureEventType::ComboRelease) {
                assert(event.value == expectedMask);
                ++comboRelease;
            } else if (event.type == ButtonGestureEventType::SinglePress) {
                ++singlePress;
            } else if (event.type == ButtonGestureEventType::DoublePress) {
                ++doublePress;
            }
        }
    }
};

struct ConfigModeEventCounts {
    uint16_t configPress = 0;
    uint16_t swingPress = 0;
    uint16_t swingRelease = 0;
    uint16_t otherComboPress = 0;
    uint16_t singlePress = 0;
    uint16_t doublePress = 0;

    void add(const ButtonGestureEvents &events, uint8_t configMask, uint8_t swingMask) {
        for (uint8_t index = 0; index < events.count; ++index) {
            const ButtonGestureEvent &event = events.items[index];
            if (event.type == ButtonGestureEventType::ComboPress) {
                if (event.value == configMask) {
                    ++configPress;
                } else if (event.value == swingMask) {
                    ++swingPress;
                } else {
                    ++otherComboPress;
                }
            } else if (event.type == ButtonGestureEventType::ComboRelease &&
                       event.value == swingMask) {
                ++swingRelease;
            } else if (event.type == ButtonGestureEventType::SinglePress) {
                ++singlePress;
            } else if (event.type == ButtonGestureEventType::DoublePress) {
                ++doublePress;
            }
        }
    }
};

constexpr uint8_t controlButton(uint8_t controlIndex) {
    return static_cast<uint8_t>(NUM_VIRTUAL_BUTTONS + controlIndex);
}

void testLegacySingleAndDoublePresses() {
    ButtonGestureInterpreter gestures;
    gestures.reset();

    gestures.updateButton(0, true, 10);
    ButtonGestureEvents firstRelease = gestures.updateButton(0, false, 50);
    assert(hasEvent(firstRelease, ButtonGestureEventType::PhysicalRelease, 0));
    assert(hasEvent(firstRelease, ButtonGestureEventType::SinglePress, 0));

    gestures.updateButton(0, true, 100);
    ButtonGestureEvents secondRelease = gestures.updateButton(0, false, 150);
    assert(hasEvent(secondRelease, ButtonGestureEventType::DoublePress, 0));
}

void testExclusiveControlDoublePresses() {
    constexpr uint8_t control3 = NUM_VIRTUAL_BUTTONS + 3;
    ButtonGestureInterpreter gestures;
    gestures.reset();

    gestures.updateButton(control3, true, 10);
    ButtonGestureEvents release = gestures.updateButton(control3, false, 50);
    assert(!hasEvent(release, ButtonGestureEventType::SinglePress, control3));
    assert(!hasEvent(gestures.flushDeferred(349), ButtonGestureEventType::SinglePress, control3));
    assert(hasEvent(gestures.flushDeferred(350), ButtonGestureEventType::SinglePress, control3));

    gestures.reset();
    gestures.updateButton(control3, true, 10);
    gestures.updateButton(control3, false, 50);
    gestures.updateButton(control3, true, 100);
    ButtonGestureEvents doubleRelease = gestures.updateButton(control3, false, 150);
    assert(hasEvent(doubleRelease, ButtonGestureEventType::DoublePress, control3));
    assert(!hasEvent(gestures.flushDeferred(500), ButtonGestureEventType::SinglePress, control3));
}

void testCtrl5PressesAreImmediateSingles() {
    constexpr uint8_t control5 = NUM_VIRTUAL_BUTTONS + 5;
    ButtonGestureInterpreter gestures;
    gestures.reset();

    gestures.updateButton(control5, true, 10);
    ButtonGestureEvents firstRelease = gestures.updateButton(control5, false, 50);
    assert(hasEvent(firstRelease, ButtonGestureEventType::SinglePress, control5));
    assert(!hasEvent(firstRelease, ButtonGestureEventType::DoublePress, control5));

    gestures.updateButton(control5, true, 100);
    ButtonGestureEvents secondRelease = gestures.updateButton(control5, false, 150);
    assert(hasEvent(secondRelease, ButtonGestureEventType::SinglePress, control5));
    assert(!hasEvent(secondRelease, ButtonGestureEventType::DoublePress, control5));
    assert(!hasEvent(gestures.flushDeferred(500), ButtonGestureEventType::SinglePress, control5));
}

void testLongPressConfirmation() {
    ButtonGestureInterpreter gestures;
    gestures.reset();

    gestures.updateButton(2, true, 10);
    assert(!hasEvent(gestures.updateButton(2, true, 509),
                     ButtonGestureEventType::LongPressArmed, 2));
    assert(hasEvent(gestures.updateButton(2, true, 510),
                    ButtonGestureEventType::LongPressArmed, 2));
    gestures.updateButton(2, false, 520);
    assert(gestures.pendingConfirmationIndex() == 2);

    gestures.updateButton(2, true, 600);
    ButtonGestureEvents confirmation = gestures.updateButton(2, false, 650);
    assert(hasEvent(confirmation, ButtonGestureEventType::ConfirmationCancelled, 2));
    assert(hasEvent(confirmation, ButtonGestureEventType::LongPressConfirmed, 2));
    assert(gestures.pendingConfirmationIndex() == -1);

    gestures.updateButton(2, true, 700);
    gestures.updateButton(2, true, 1200);
    gestures.updateButton(2, false, 1210);
    ButtonGestureEvents cancellation = gestures.updateButton(3, true, 1300);
    assert(hasEvent(cancellation, ButtonGestureEventType::ConfirmationCancelled, 3));
}

void testSettledChordConsumesSoloReleases() {
    constexpr uint8_t control0 = NUM_VIRTUAL_BUTTONS;
    constexpr uint8_t control1 = NUM_VIRTUAL_BUTTONS + 1;
    constexpr uint8_t chord = (1U << 0) | (1U << 1);
    ButtonGestureInterpreter gestures;
    gestures.reset();

    gestures.updateButton(control0, true, 10);
    gestures.updateButton(control1, true, 10);
    gestures.updateControlMask(chord, 10);
    assert(!hasEvent(gestures.updateControlMask(chord, 89),
                     ButtonGestureEventType::ComboPress, chord));
    assert(hasEvent(gestures.updateControlMask(chord, 90),
                    ButtonGestureEventType::ComboPress, chord));

    ButtonGestureEvents release0 = gestures.updateButton(control0, false, 100);
    gestures.updateControlMask(1U << 1, 100);
    ButtonGestureEvents release1 = gestures.updateButton(control1, false, 110);
    gestures.updateControlMask(0, 110);
    assert(!hasEvent(release0, ButtonGestureEventType::SinglePress, control0));
    assert(!hasEvent(release1, ButtonGestureEventType::SinglePress, control1));
}

void testEveryTwoButtonPressAndReleaseOrder() {
    constexpr uint8_t arpChord = (1U << 2) | (1U << 4);
    constexpr uint8_t swingChord = (1U << 2) | (1U << 3);

    for (uint8_t first = 0; first < NUM_CONTROL_BUTTONS; ++first) {
        for (uint8_t second = static_cast<uint8_t>(first + 1);
             second < NUM_CONTROL_BUTTONS; ++second) {
            const uint8_t chord = static_cast<uint8_t>((1U << first) | (1U << second));
            const bool specialChord = chord == arpChord || chord == swingChord;
            for (uint8_t pressOrder = 0; pressOrder < 2; ++pressOrder) {
                for (uint8_t releaseOrder = 0; releaseOrder < 2; ++releaseOrder) {
                    const uint8_t pressedFirst = pressOrder == 0 ? first : second;
                    const uint8_t pressedSecond = pressOrder == 0 ? second : first;
                    const uint8_t releasedFirst = releaseOrder == 0 ? first : second;
                    const uint8_t releasedSecond = releaseOrder == 0 ? second : first;
                    ButtonGestureInterpreter gestures;
                    EventCounts counts;
                    gestures.reset();

                    counts.add(gestures.updateButton(controlButton(pressedFirst), true, 10), chord);
                    counts.add(gestures.updateControlMask(1U << pressedFirst, 10), chord);
                    counts.add(gestures.updateButton(controlButton(pressedSecond), true, 30), chord);
                    counts.add(gestures.updateControlMask(chord, 30), chord);
                    counts.add(gestures.updateControlMask(chord, 109), chord);
                    assert(counts.comboPress == 0);
                    if (!specialChord) {
                        counts.add(gestures.updateControlMask(chord, 110), chord);
                        assert(counts.comboPress == 1);
                    }

                    counts.add(gestures.updateButton(controlButton(releasedFirst), false, 140), chord);
                    counts.add(gestures.updateControlMask(1U << releasedSecond, 140), chord);
                    counts.add(gestures.updateButton(controlButton(releasedSecond), false, 150), chord);
                    counts.add(gestures.updateControlMask(0, 150), chord);

                    assert(counts.comboPress == 1);
                    assert(counts.comboRelease == (specialChord ? 1 : 0));
                    assert(counts.singlePress == 0);
                    assert(counts.doublePress == 0);
                }
            }
        }
    }
}

void testEveryThreeButtonRollingPressAndReleaseOrder() {
    constexpr std::array<std::array<uint8_t, 3>, 6> permutations = {{{0, 1, 2},
                                                                      {0, 2, 1},
                                                                      {1, 0, 2},
                                                                      {1, 2, 0},
                                                                      {2, 0, 1},
                                                                      {2, 1, 0}}};
    constexpr uint8_t chord = (1U << 0) | (1U << 1) | (1U << 2);

    for (const auto &pressOrder : permutations) {
        for (const auto &releaseOrder : permutations) {
            ButtonGestureInterpreter gestures;
            EventCounts counts;
            uint8_t mask = 0;
            unsigned long now = 10;
            gestures.reset();

            for (uint8_t control : pressOrder) {
                mask = static_cast<uint8_t>(mask | (1U << control));
                counts.add(gestures.updateButton(controlButton(control), true, now), chord);
                counts.add(gestures.updateControlMask(mask, now), chord);
                now += 20;
            }

            counts.add(gestures.updateControlMask(chord, 129), chord);
            assert(counts.comboPress == 0);
            counts.add(gestures.updateControlMask(chord, 130), chord);
            assert(counts.comboPress == 1);

            now = 150;
            mask = chord;
            for (uint8_t control : releaseOrder) {
                mask = static_cast<uint8_t>(mask & ~(1U << control));
                counts.add(gestures.updateButton(controlButton(control), false, now), chord);
                counts.add(gestures.updateControlMask(mask, now), chord);
                now += 10;
            }

            assert(counts.comboPress == 1);
            assert(counts.comboRelease == 0);
            assert(counts.singlePress == 0);
            assert(counts.doublePress == 0);
        }
    }
}

void testEveryConfigModePressAndReleasePermutationSuppressesSwingSubset() {
    constexpr uint8_t swingChord = (1U << 2) | (1U << 3);
    constexpr uint8_t configChord = (1U << 0) | (1U << 2) | (1U << 3) | (1U << 5);
    const std::array<uint8_t, 4> controls = {0, 2, 3, 5};
    std::array<uint8_t, 4> pressOrder = controls;

    do {
        std::array<uint8_t, 4> releaseOrder = controls;
        do {
            ButtonGestureInterpreter gestures;
            ConfigModeEventCounts counts;
            uint8_t mask = 0;
            unsigned long now = 10;
            gestures.reset();

            for (uint8_t control : pressOrder) {
                mask = static_cast<uint8_t>(mask | (1U << control));
                counts.add(gestures.updateButton(controlButton(control), true, now), configChord,
                           swingChord);
                counts.add(gestures.updateControlMask(mask, now), configChord, swingChord);
                now += 20;
            }

            counts.add(gestures.updateControlMask(configChord, 149), configChord, swingChord);
            assert(counts.configPress == 0);
            counts.add(gestures.updateControlMask(configChord, 150), configChord, swingChord);
            assert(counts.configPress == 1);

            now = 170;
            mask = configChord;
            for (uint8_t control : releaseOrder) {
                mask = static_cast<uint8_t>(mask & ~(1U << control));
                counts.add(gestures.updateButton(controlButton(control), false, now), configChord,
                           swingChord);
                counts.add(gestures.updateControlMask(mask, now), configChord, swingChord);
                now += 10;
            }

            assert(counts.configPress == 1);
            assert(counts.swingPress == 0);
            assert(counts.swingRelease == 0);
            assert(counts.otherComboPress == 0);
            assert(counts.singlePress == 0);
            assert(counts.doublePress == 0);
        } while (std::next_permutation(releaseOrder.begin(), releaseOrder.end()));
    } while (std::next_permutation(pressOrder.begin(), pressOrder.end()));
}

void testIntentionalSubCombosRearmAfterAllControlsAreReleased() {
    constexpr uint8_t swingChord = (1U << 2) | (1U << 3);
    constexpr uint8_t argPairChord = (1U << 0) | (1U << 2);
    constexpr uint8_t configChord = (1U << 0) | (1U << 2) | (1U << 3) | (1U << 5);
    ButtonGestureInterpreter gestures;
    gestures.reset();

    uint8_t mask = 0;
    unsigned long now = 10;
    for (uint8_t control : std::array<uint8_t, 4>{2, 3, 0, 5}) {
        mask = static_cast<uint8_t>(mask | (1U << control));
        gestures.updateButton(controlButton(control), true, now);
        gestures.updateControlMask(mask, now);
        now += 20;
    }
    assert(hasEvent(gestures.updateControlMask(configChord, 150),
                    ButtonGestureEventType::ComboPress, configChord));

    now = 170;
    for (uint8_t control : std::array<uint8_t, 4>{0, 5, 2, 3}) {
        mask = static_cast<uint8_t>(mask & ~(1U << control));
        gestures.updateButton(controlButton(control), false, now);
        ButtonGestureEvents release = gestures.updateControlMask(mask, now);
        assert(!hasEvent(release, ButtonGestureEventType::ComboPress, swingChord));
        now += 10;
    }
    assert(mask == 0);

    // A fresh short Swing chord must work immediately after the all-up boundary.
    gestures.updateButton(controlButton(2), true, 300);
    gestures.updateControlMask(1U << 2, 300);
    gestures.updateButton(controlButton(3), true, 320);
    gestures.updateControlMask(swingChord, 320);
    gestures.updateButton(controlButton(2), false, 380);
    ButtonGestureEvents shortSwing = gestures.updateControlMask(1U << 3, 380);
    assert(hasEvent(shortSwing, ButtonGestureEventType::ComboPress, swingChord));
    assert(hasEvent(shortSwing, ButtonGestureEventType::ComboRelease, swingChord));
    gestures.updateButton(controlButton(3), false, 390);
    gestures.updateControlMask(0, 390);

    // Ordinary settled sub-combos must also re-arm after returning to all-up.
    gestures.updateButton(controlButton(0), true, 500);
    gestures.updateControlMask(1U << 0, 500);
    gestures.updateButton(controlButton(2), true, 520);
    gestures.updateControlMask(argPairChord, 520);
    assert(hasEvent(gestures.updateControlMask(argPairChord, 600),
                    ButtonGestureEventType::ComboPress, argPairChord));
    gestures.updateButton(controlButton(0), false, 610);
    gestures.updateControlMask(1U << 2, 610);
    gestures.updateButton(controlButton(2), false, 620);
    gestures.updateControlMask(0, 620);

    // The long form shares the same re-arm boundary and retains its release.
    gestures.updateButton(controlButton(2), true, 700);
    gestures.updateControlMask(1U << 2, 700);
    gestures.updateButton(controlButton(3), true, 720);
    gestures.updateControlMask(swingChord, 720);
    assert(hasEvent(gestures.updateControlMask(swingChord, 1220),
                    ButtonGestureEventType::ComboLongPress, swingChord));
    gestures.updateButton(controlButton(2), false, 1230);
    ButtonGestureEvents longSwingRelease = gestures.updateControlMask(1U << 3, 1230);
    assert(!hasEvent(longSwingRelease, ButtonGestureEventType::ComboPress, swingChord));
    assert(hasEvent(longSwingRelease, ButtonGestureEventType::ComboRelease, swingChord));
}

void testShortAndLongSpecialChords() {
    constexpr uint8_t arpChord = (1U << 2) | (1U << 4);
    ButtonGestureInterpreter gestures;
    gestures.reset();

    gestures.updateControlMask(arpChord, 10);
    ButtonGestureEvents shortRelease = gestures.updateControlMask(0, 200);
    assert(hasEvent(shortRelease, ButtonGestureEventType::ComboPress, arpChord));
    assert(hasEvent(shortRelease, ButtonGestureEventType::ComboRelease, arpChord));

    gestures.reset();
    gestures.updateControlMask(arpChord, 10);
    ButtonGestureEvents held = gestures.updateControlMask(arpChord, 510);
    assert(hasEvent(held, ButtonGestureEventType::ComboLongPress, arpChord));
    ButtonGestureEvents longRelease = gestures.updateControlMask(0, 520);
    assert(!hasEvent(longRelease, ButtonGestureEventType::ComboPress, arpChord));
    assert(hasEvent(longRelease, ButtonGestureEventType::ComboRelease, arpChord));

    gestures.reset();
    gestures.updateControlMask(arpChord, 10);
    assert(hasEvent(gestures.updateControlMask(arpChord, 510),
                    ButtonGestureEventType::ComboLongPress, arpChord));
    const uint8_t arpSuperset = static_cast<uint8_t>(arpChord | (1U << 0));
    assert(!hasEvent(gestures.updateControlMask(arpSuperset, 520),
                     ButtonGestureEventType::ComboRelease, arpChord));
    assert(!hasEvent(gestures.updateControlMask(arpChord, 530),
                     ButtonGestureEventType::ComboRelease, arpChord));
    ButtonGestureEvents supersetRelease = gestures.updateControlMask(1U << 2, 540);
    assert(!hasEvent(supersetRelease, ButtonGestureEventType::ComboPress, arpChord));
    assert(hasEvent(supersetRelease, ButtonGestureEventType::ComboRelease, arpChord));
}
} // namespace

int main() {
    testLegacySingleAndDoublePresses();
    testExclusiveControlDoublePresses();
    testCtrl5PressesAreImmediateSingles();
    testLongPressConfirmation();
    testSettledChordConsumesSoloReleases();
    testEveryTwoButtonPressAndReleaseOrder();
    testEveryThreeButtonRollingPressAndReleaseOrder();
    testEveryConfigModePressAndReleasePermutationSuppressesSwingSubset();
    testIntentionalSubCombosRearmAfterAllControlsAreReleased();
    testShortAndLongSpecialChords();
    return 0;
}
