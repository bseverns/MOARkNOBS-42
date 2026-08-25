# MN42 Object Card

MOARkNOBS-42 is a specific open, reactive MIDI/OSC performance instrument. It is not a generic controller platform and not a minimal faderbank.

Current repo status: hardware-test package. For folder ownership and generated-output boundaries, see [Repository Contents](../project/RepositoryContents.md).

## What Is In The Object?

- 42 virtual control slots
- six envelope follower inputs
- Teensy 4.0 firmware
- MIDI output lanes over USB and hardware MIDI
- RGB LED feedback
- display/status surfaces
- browser configurator support
- optional desktop Bridge for OSC and virtual MIDI workflows

## What Are The Controls?

The main performance model is slot-based. A slot can represent a MIDI behavior, store per-slot settings, and receive modulation from reactive sources.

The reactive sources include:

- envelope follower input
- ARG pair math between envelope followers
- LFO routes
- live hardware control movement
- profile-owned incoming MIDI for supported machine parameters and eligible direct, unmodulated CC slots

These sources do not all participate through one universal resolver. Reactive composition and external-MIDI takeover have separate, documented eligibility and ownership semantics. See [Who Controls This Slot?](../learn/OneSignalPath.md).

## What Comes Out?

MN42 can emit MIDI messages from slot state and modulation. The documented system also supports OSC and virtual MIDI workflows through the Bridge.

The current docs are conservative about host support. Read [Host Compatibility](../reference/HostCompatibility.md) before treating any browser, DAW, or operating-system path as generally verified.

## What Connects To It?

- USB for firmware/configurator paths
- hardware MIDI connections
- signal inputs for envelope follower behavior
- browser configurator over the supported transport path
- local Bridge process when OSC, virtual MIDI, or App-over-Bridge is the better fit

## What Can Be Configured Without Recompiling?

Many performance and mapping behaviors can be configured without recompiling firmware:

- slot message type, channel, and primary value fields
- profiles
- EF assignment and follower settings
- ARG settings
- LFO route targets and ranges
- profile-owned incoming MIDI bindings, response modes, ranges, and takeover policy
- bridge/session workflow settings

See [Configure Without Recompiling](ConfigureWithoutRecompiling.md).

## What Is Experimental Or Hardware-Test Only?

This repo is still framed as a hardware-test package. Hardware-test evidence matters more than optimistic interpretation.

Treat these as conservative boundaries:

- public fabrication readiness is not claimed
- rail topology verification is not implied unless a dated evidence doc says so
- host/DAW compatibility is setup-specific unless covered by receipts
- release status depends on [Release Criteria](../release/ReleaseCriteria.md)

## What Should Not Be Assumed Yet?

Do not assume universal browser support, universal DAW routing, signed bridge installers, fabrication-ready production artifacts, or production-safe hardware variants just because a software check passes.

The instrument is real and documented, but the proof path is deliberately visible.
