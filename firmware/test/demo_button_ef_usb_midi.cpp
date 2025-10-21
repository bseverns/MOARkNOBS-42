/*
 * Button + Envelope demo sketch
 * ------------------------------
 * Flash this when you want a barebones hardware riff: one control button
 * and one envelope follower slinging USB MIDI without the rest of the rig.
 *
 * Wiring assumptions:
 *   - Control button on pin 12, active LOW (use the board's stock control btn 0)
 *   - Envelope follower output patched into A0
 *
 * Upload with:
 *   pio run -d firmware -e teensy40_button_ef_demo -t upload
 */

#include <Arduino.h>
#include <math.h>
#include <usb_midi.h>

namespace {
constexpr uint8_t kButtonPin = 12;   // control button #0
constexpr uint8_t kEnvelopePin = A0; // envelope follower jack
constexpr uint8_t kMidiChannel = 1;  // 1-based MIDI channel
constexpr uint8_t kNote = 60;        // middle C when the button fires
constexpr uint8_t kEnvelopeCC = 21;  // CC number carrying the envelope
constexpr uint32_t kCcIntervalMs = 8;
constexpr float kBaselineLerp = 0.0025f; // slow drift to follow noise floor
constexpr float kEnvelopeLerp = 0.18f;   // snappy EWMA for envelope peaks

float baseline = 0.0f;
float envelope = 0.0f;
uint8_t lastCcValue = 0xFF;
bool noteActive = false;
uint32_t lastCcStamp = 0;

uint16_t readRawEnvelope() { return analogRead(kEnvelopePin); }

void calibrateBaseline() {
    uint32_t total = 0;
    constexpr uint16_t samples = 128;
    for (uint16_t i = 0; i < samples; ++i) {
        total += readRawEnvelope();
        delayMicroseconds(50);
    }
    baseline = static_cast<float>(total) / samples;
    envelope = 0.0f;
}

uint8_t levelToMidi(float level) {
    level = constrain(level, 0.0f, 1023.0f);
    return static_cast<uint8_t>(roundf((level / 1023.0f) * 127.0f));
}

void pumpEnvelope() {
    float raw = static_cast<float>(readRawEnvelope());
    baseline = (1.0f - kBaselineLerp) * baseline + kBaselineLerp * raw;
    float delta = raw - baseline;
    if (delta < 0.0f)
        delta = 0.0f;
    envelope = (1.0f - kEnvelopeLerp) * envelope + kEnvelopeLerp * delta;
}

void maybeSendEnvelope() {
    uint32_t now = millis();
    if (now - lastCcStamp < kCcIntervalMs)
        return;
    lastCcStamp = now;
    uint8_t value = levelToMidi(envelope);
    if (value == lastCcValue)
        return;
    lastCcValue = value;
    usbMIDI.sendControlChange(kEnvelopeCC, value, kMidiChannel);
}

void serviceButton() {
    bool pressed = digitalRead(kButtonPin) == LOW;
    if (pressed && !noteActive) {
        usbMIDI.sendNoteOn(kNote, 100, kMidiChannel);
        noteActive = true;
    } else if (!pressed && noteActive) {
        usbMIDI.sendNoteOff(kNote, 0, kMidiChannel);
        noteActive = false;
    }
}

void drainUsbMidi() {
    while (usbMIDI.read()) {
        // We don't react to incoming packets in this sketch; just keep
        // the Teensy USB stack from clogging.
    }
}

} // namespace

void setup() {
    pinMode(kButtonPin, INPUT_PULLUP);
    pinMode(kEnvelopePin, INPUT);
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Give USB a heartbeat on laptop-powered rigs.
    }
    Serial.println("MOARkNOBS: 1-button/1-EF USB MIDI demo");
    Serial.println("Hold things quiet for a sec — calibrating baseline...");
    calibrateBaseline();
    Serial.println("Baseline locked. Mash the button, feed the EF, and watch MIDI Monitor.");

    usbMIDI.begin();
}

void loop() {
    pumpEnvelope();
    maybeSendEnvelope();
    serviceButton();
    drainUsbMidi();
}
