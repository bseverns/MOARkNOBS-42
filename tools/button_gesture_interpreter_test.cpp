#include "ButtonGestureInterpreter.h"

#include <cassert>
#include <cstdint>

namespace {
bool hasEvent(const ButtonGestureEvents &events, ButtonGestureEventType type,
              uint8_t value) {
    for (uint8_t index = 0; index < events.count; ++index) {
        if (events.items[index].type == type && events.items[index].value == value) return true;
    }
    return false;
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
}
} // namespace

int main() {
    testLegacySingleAndDoublePresses();
    testExclusiveControlDoublePresses();
    testLongPressConfirmation();
    testSettledChordConsumesSoloReleases();
    testShortAndLongSpecialChords();
    return 0;
}
