# MIDI + DSP 101 Primer

> A crash pad for anyone about to wire MIDI into math. This is the stuff our firmware assumes you already vibe with.

## MIDI in 60 Seconds

MIDI messages are just structured bytes with attitude. Every packet starts with a **status byte** (0x8n–0xEn for channel voice messages, 0xFn for system-level), followed by up to two **data bytes**. The low nibble (`n`) of the status byte picks the channel (0–15).

### Channels

- **Sixteen lanes, zero collisions** — devices listen on channels 1–16. We default to channel 1, but MIDIHandler lets you remap quickly.
- **Omni vs. Poly modes** — some gear ignores the channel and listens to everything (omni). We stay polite and always tag the channel.
- **Channel pressure vs. poly aftertouch** — both are `0xDn` or `0xAn` types; channel pressure is the whole keyboard, poly is per-note.

**Further reading**

- [MIDI 1.0 Detailed Specification (AMEI/JMSC reprint)](https://www.midi.org/specifications/midi1-specifications/m1-v4-2-1-midi-1-0-detailed-specification) — the canonical PDF when you want byte-level truth.
- [MIDI Association Primer on Channels](https://www.midi.org/articles-old/about-midi-part-3-midi-channels) — plain-language walkthrough.

### Control Change (CC)

Control Change messages (`0xBn`) deliver two data bytes: controller number (`0–127`) and value (`0–127`). We lean on CCs to map knobs, envelope outputs, and WebSerial edits.

- **Continuous controllers** — knobs, faders, mod wheels. Most synths expect CC1 for mod wheel, CC7 for volume, CC74 for brightness.
- **Switch controllers** — buttons that spit 0/127, like sustain (CC64). We still treat them as CCs so automation lanes stay tidy.
- **High-resolution pairs** — some gear lets you combine coarse (MSB) and fine (LSB) CCs (e.g., CC0 + CC32) for 14-bit control.

**Curated resources**

- [MIDI CC Reference (Novation)](https://support.novationmusic.com/s/article/What-are-the-MIDI-CC-values-for-your-Novation-product) — practical tables with device context.
- [MIDI 1.0 Spec, Chapter 4](https://www.midi.org/specifications/midi1-specifications/m1-v4-2-1-midi-1-0-detailed-specification) — official definitions.

### RPN & NRPN

Registered Parameter Numbers (RPN) and Non-Registered Parameter Numbers (NRPN) combine multiple CC messages to address high-resolution parameters.

1. Send CC99 (NRPN MSB) or CC101 (RPN MSB).
2. Send CC98 (NRPN LSB) or CC100 (RPN LSB).
3. Use CC6 (Data Entry MSB) and optionally CC38 (LSB) to set values.
4. Optionally send CC96/97 for increment/decrement.
5. Send RPN null (`CC101=127`, `CC100=127`) or NRPN null (same pair) to stop targeting that parameter.

MN42 slots can generate RPN and NRPN sequences; the parser also keeps inbound sequences intact so external rigs stay coherent.

**Curated resources**

- [NRPN vs. RPN explainer (Sound On Sound)](https://www.soundonsound.com/techniques/midi-controllers) — context plus creative use cases.
- [MIDI Tuning Standard (RPN 0)](https://www.midi.org/articles/midi-tuning) — example of registered parameters in the wild.

### System Exclusive (SysEx)

SysEx messages start with `0xF0`, end with `0xF7`, and carry manufacturer-specific payloads. Slots now store 16-byte templates
that accept `XX` (7-bit), `MSB`, and `LSB` placeholders so you can spit out parameterised bursts without editing code. The
firmware still forwards arbitrary inbound packets, but anything longer than 64 bytes gets dropped unless you explicitly ask for
pass-through mode—self-defense against misbehaving gear.

- **Manufacturer ID** — first data byte(s) after `0xF0`. 0x7D is reserved for non-commercial DIY builds like ours.
- **Variable length** — keep messages under a few hundred bytes when routing over DIN; USB can cope with more.
- **Checksums & framing** — not in the spec, but many vendors use them. Validate before trusting the payload.

**Curated resources**

- [SysEx Made Simple (MIDI.org)](https://www.midi.org/articles-old/sysex-messages) — hand-holding introduction.
- [Teensy MIDI SysEx example](https://www.pjrc.com/teensy/td_midi.html#sysex) — quick code patterns for embedded devices.

## DSP Moves We Lean On

The firmware steals classic audio tricks to keep modulation musical and stable.

### Envelope Followers

Envelope followers track the amplitude of an incoming signal, returning a smoothed control voltage or value.

- **Rectify + smooth** — we bias the audio up, rectify it, then feed a two-stage RC or digital smoothing network.
- **Attack/Release games** — fast attack (tiny RC or high alpha) for snappy hits, slower release so the envelope glides down.
- **Calibration** — our code samples a reference pin and subtracts offsets so silence reads as zero.

**How it shows up here**

- `EnvelopeFollower::update()` in firmware samples the ADC, oversamples, and optionally feeds a BiquadFilter before scaling to MIDI.
- ARG mode blends two followers using math operators (sum, difference, random) for pseudo-modular mayhem.

**Further reading**

- [Envelopes and Envelope Followers (Ableton Learning Synths)](https://learningsynths.ableton.com/modulation/envelopes) — animated explanation for visual thinkers.
- [Practical Envelope Followers (Electronics-Tutorials.ws)](https://www.electronics-tutorials.ws/filter/envelope.html) — analog circuit math with diagrams.

### Biquad Filters

Biquads are second-order IIR filters with two poles and two zeros. They're light on CPU and perfect for envelope cleanup.

- **Coefficient sets** — we precompute `a0–a2` and `b1–b2` using the [RBJ cookbook](https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html).
- **Stateful** — each call to `process()` stores the previous two input/output samples for continuity.
- **Modes we use** — low-pass to smooth, high-pass to shave DC, band-pass for resonant modulation.

**How it shows up here**

- `BiquadFilter::configure()` resets state so new parameters don't inherit old energy.
- Envelope followers lean on the low-, high-, and band-pass modes; the random and exponential options are handled digitally afterward.

**Further reading**

- [RBJ Audio EQ Cookbook](https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html) — original derivations and formulas.
- [EarLevel Engineering: Biquad Calculator](https://www.earlevel.com/main/2013/10/13/biquad-calculator-v2/) — tweakable calculator with code snippets.

## Cheatsheet Links

Keep these in reach when writing docs or firmware:

- [MIDI Association Resource Library](https://www.midi.org/section/midi-articles) — official primers and specs.
- [Teensy MIDI Library Reference](https://www.pjrc.com/teensy/td_midi.html) — embedded-focused examples for every message type.
- [Audio EQ Cookbook](https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html) — quick coefficients without hunting textbooks.
- [Open Music Labs: Envelope Followers](http://www.openmusiclabs.com/learning/tutorials/envelopefollower/) — deep-dive with schematics.

Document edits? Drop them here and in the [Docs Index](../index.md) so the tribe knows the primer stayed fresh.
