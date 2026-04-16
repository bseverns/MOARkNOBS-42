#include <Arduino.h>
#include <usb_midi.h>

namespace {

enum class RandomMidiMode : uint8_t { ControlChange = 0, Note = 1, Mixed = 2 };

#if defined(RANDOM_USB_MIDI_MODE_NOTE)
constexpr RandomMidiMode kMode = RandomMidiMode::Note;
#elif defined(RANDOM_USB_MIDI_MODE_MIXED)
constexpr RandomMidiMode kMode = RandomMidiMode::Mixed;
#else
constexpr RandomMidiMode kMode = RandomMidiMode::ControlChange;
#endif

#ifndef RANDOM_USB_MIDI_INTERVAL_MS
#define RANDOM_USB_MIDI_INTERVAL_MS 60
#endif

#ifndef RANDOM_USB_MIDI_NOTE_LENGTH_MS
#define RANDOM_USB_MIDI_NOTE_LENGTH_MS 140
#endif

constexpr uint32_t kEventIntervalMs = RANDOM_USB_MIDI_INTERVAL_MS;
constexpr uint32_t kNoteLengthMs = RANDOM_USB_MIDI_NOTE_LENGTH_MS;
constexpr uint32_t kStatusIntervalMs = 1000;
constexpr uint8_t kMinChannel = 1;
constexpr uint8_t kMaxChannel = 16;
constexpr uint8_t kMinCcNumber = 0;
constexpr uint8_t kMaxCcNumber = 119; // keep clear of channel mode messages (120-127)
constexpr uint8_t kMinValue = 0;
constexpr uint8_t kMaxValue = 127;
constexpr uint8_t kMinNote = 24;
constexpr uint8_t kMaxNote = 96;

bool g_noteActive = false;
uint8_t g_activeNote = kMinNote;
uint8_t g_activeChannel = kMinChannel;
uint32_t g_lastEventAtMs = 0;
uint32_t g_noteOffAtMs = 0;
uint32_t g_lastStatusAtMs = 0;
uint32_t g_ccSent = 0;
uint32_t g_notesOnSent = 0;
uint32_t g_notesOffSent = 0;

uint8_t randomInclusive(uint8_t minValue, uint8_t maxValue) {
    return static_cast<uint8_t>(
        random(static_cast<long>(minValue), static_cast<long>(maxValue) + 1));
}

const char *modeLabel() {
    switch (kMode) {
    case RandomMidiMode::ControlChange:
        return "CC";
    case RandomMidiMode::Note:
        return "NOTE";
    case RandomMidiMode::Mixed:
        return "MIXED";
    default:
        return "UNKNOWN";
    }
}

void drainIncomingUsbMidi() {
    while (usbMIDI.read()) {
    }
}

void maybeSendNoteOff(uint32_t nowMs) {
    if (!g_noteActive)
        return;
    if (static_cast<int32_t>(nowMs - g_noteOffAtMs) < 0)
        return;

    usbMIDI.sendNoteOff(g_activeNote, 0, g_activeChannel);
    ++g_notesOffSent;
    g_noteActive = false;
}

void emitRandomControlChange() {
    const uint8_t channel = randomInclusive(kMinChannel, kMaxChannel);
    const uint8_t ccNumber = randomInclusive(kMinCcNumber, kMaxCcNumber);
    const uint8_t value = randomInclusive(kMinValue, kMaxValue);
    usbMIDI.sendControlChange(ccNumber, value, channel);
    ++g_ccSent;
}

void emitRandomNote(uint32_t nowMs) {
    maybeSendNoteOff(nowMs);

    const uint8_t channel = randomInclusive(kMinChannel, kMaxChannel);
    const uint8_t note = randomInclusive(kMinNote, kMaxNote);
    const uint8_t velocity = randomInclusive(40, kMaxValue);

    usbMIDI.sendNoteOn(note, velocity, channel);
    ++g_notesOnSent;
    g_activeChannel = channel;
    g_activeNote = note;
    g_noteActive = true;
    g_noteOffAtMs = nowMs + kNoteLengthMs;
}

void emitEvent(uint32_t nowMs) {
    if (kMode == RandomMidiMode::ControlChange) {
        emitRandomControlChange();
        return;
    }
    if (kMode == RandomMidiMode::Note) {
        emitRandomNote(nowMs);
        return;
    }

    if (random(0, 2) == 0)
        emitRandomControlChange();
    else
        emitRandomNote(nowMs);
}

void printStatus(uint32_t nowMs) {
    if (nowMs - g_lastStatusAtMs < kStatusIntervalMs)
        return;
    g_lastStatusAtMs = nowMs;

    Serial.printf(
        "[USB-MIDI-RANDOM] mode=%s intervalMs=%lu noteLengthMs=%lu cc=%lu noteOn=%lu noteOff=%lu "
        "activeNote=%s\n",
        modeLabel(), static_cast<unsigned long>(kEventIntervalMs),
        static_cast<unsigned long>(kNoteLengthMs), static_cast<unsigned long>(g_ccSent),
        static_cast<unsigned long>(g_notesOnSent), static_cast<unsigned long>(g_notesOffSent),
        g_noteActive ? "yes" : "no");
}

} // namespace

void setup() {
    Serial.begin(115200);
    const uint32_t startMs = millis();
    while (!Serial && (millis() - startMs) < 1500) {
        delay(5);
    }

    const uint32_t seed = analogRead(A0) ^ analogRead(A1) ^ micros();
    randomSeed(seed);
    usbMIDI.begin();

    Serial.println("USB MIDI random generator started.");
    Serial.printf("[USB-MIDI-RANDOM] mode=%s seed=%lu\n", modeLabel(),
                  static_cast<unsigned long>(seed));
}

void loop() {
    const uint32_t nowMs = millis();

    if (nowMs - g_lastEventAtMs >= kEventIntervalMs) {
        g_lastEventAtMs = nowMs;
        emitEvent(nowMs);
    }

    maybeSendNoteOff(nowMs);
    printStatus(nowMs);
    drainIncomingUsbMidi();
}
