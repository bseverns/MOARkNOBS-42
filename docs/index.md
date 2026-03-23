# MOARkNOBS-42

![MOARkNOBS-42 controller on the bench showing the button grid, knob field, and display area.](land.png){ .hero-image }

<div class="mn42-landing-hero" markdown="1">

Small-batch MIDI/OSC performance instrument

MOARkNOBS-42 is a documentation-rich hardware instrument for artists, builders, and instrument hackers who want a controller they can understand, modify, and keep learning from. It combines a physical control surface, open firmware, a browser configurator, a bridge for OSC and virtual MIDI, and a public paper trail for how the whole thing works.

[Join pilot run / interest list](PilotRun.md){ .md-button .md-button--primary }
[See features](#core-features){ .md-button }
[Start here / learn how it works](GuidedRoutes.md){ .md-button }

</div>

## What it is

MOARkNOBS-42 is a small-batch MIDI/OSC performance instrument with a surrounding ecosystem, not just a bare controller board.

The object in your hands is only part of the story. The repo also supports:

- a browser configurator for setup, monitoring, and profile management
- on-device profiles so working states can be recalled instead of rebuilt from scratch
- a bridge path for OSC and DAW-facing virtual MIDI workflows
- builder, performer, validation, and troubleshooting docs that explain what is actually supported

If you want the short outsider-friendly version first, read [Why MN42](WhyMN42.md).

## Why it feels different

Many controllers are designed to disappear behind presets, hidden mappings, or sealed software stacks. MOARkNOBS-42 goes the other direction.

- The hardware, firmware, configurator, and bridge are documented as one legible system.
- The instrument is open enough to rebuild, audit, and remix rather than merely consume.
- The docs are part of the instrument, not a last-minute appendix.
- Validation, support boundaries, and unverified areas are called out directly instead of buried under marketing language.

The goal is not mass-market smoothness. The goal is an instrument whose control logic stays visible.

## Core features

<div class="grid cards mn42-card-grid" markdown="1">

- **42 virtual slots**

  The instrument centers around 42 virtual slots that can be mapped, recalled, and watched as part of the same runtime system.

- **Six envelope followers**

  Six reactive inputs let the rig respond to real signal activity instead of only static knob gestures.

- **Browser configurator**

  The configurator handles setup, monitoring, staged edits, and profile work in plain language over the documented device contract.

- **Profiles and recall**

  Profile slots `A` through `D` let you save, load, reset, and back up working states instead of rebuilding a setup live.

- **OSC / virtual MIDI bridge**

  The bridge exposes OSC and a DAW-facing virtual MIDI path, and now includes a browser-driven local console.

- **Dual MIDI hardware formats**

  Current repo docs describe both 5-pin DIN and 1/8" TRS Type-A MIDI hardware support.

</div>

## Who it's for

<div class="grid cards mn42-card-grid" markdown="1">

- **Experimental musicians**

  For performers who want the controller to be part of the instrument, not just a utility layer.

- **Sound and media artists**

  For people working across browser tools, OSC hosts, DAWs, installation contexts, or custom performance setups.

- **Advanced builders**

  For people comfortable with fabrication files, flashing firmware, and validating real hardware behavior on the bench.

- **Educators and instrument hackers**

  For workshops, studios, and classrooms where openness, legibility, and rebuildability matter as much as the final sound.

</div>

This is best suited to people who are comfortable exploring systems rather than expecting every edge case to be hidden behind a wizard.

## Pilot Run / Artist Edition

The right frame for a first small-batch release is a pilot run: an artist-run instrument release with a documented ecosystem around it, not a generic consumer launch.

What that currently means in this project:

- the instrument hardware itself
- the current firmware and its documented behavior
- the browser configurator workflow
- the OSC / virtual MIDI bridge workflow
- the builder, performer, validation, and troubleshooting docs that explain how to use and verify it

What it does not mean:

- implied mass-market support coverage
- undocumented host compatibility guarantees
- invented timelines, pricing, or bundle promises that are not yet published

See [Pilot Run / Artist Edition](PilotRun.md) for the project-specific explanation.

## What it is not

<div class="mn42-callout mn42-callout-not" markdown="1">

This is not a generic plug-and-play controller for everyone.

It is not framed here as a sealed appliance, a beginner-friendly consumer gadget, or a promise that every host/workflow has already been validated. It is a particular kind of open, teachable performance instrument for people who value legibility, modification, and documented behavior.

</div>

## Support boundary

<div class="mn42-callout mn42-callout-support" markdown="1">

Support here is docs-first and artist-run.

The repo publishes workflows, validation guidance, and support boundaries clearly, but it does not promise consumer-style warranty handling, substitute-part approval, or universal one-on-one setup support. The practical boundary is described in [License and Support](LicenseAndSupport.md).

</div>

## Choose your next step

<div class="grid cards mn42-card-grid" markdown="1">

- **Quickstart for Builders**

  Ordering parts, flashing firmware, and first bring-up: [Quickstart for Builders](QuickstartForBuilders.md)

- **Quickstart for Performers**

  Connect the board, use the configurator, and work with profiles: [Quickstart for Performers](QuickstartForPerformers.md)

- **Connectivity Guide**

  Decide between direct WebSerial and the bridge path: [Connectivity Guide](ConnectivityGuide.md)

- **Validation Flow**

  Use the conservative go/no-go path from bring-up to demo-ready: [Validation Flow](ValidationFlow.md)

- **Demo Test Punch List**

  Run a real-world prototype or rehearsal pass: [Demo Test Punch List](DemoTestPunchList.md)

- **Guided Routes and deep docs**

  Choose a learning path or dive into the dense technical library: [Guided Routes](GuidedRoutes.md) and [Docs Guide](DocsGuide.md)

</div>

## FAQ

??? question "Do I need to code?"

    Not necessarily. A performer can use the configurator and profile workflow without changing firmware. A builder or remixer will get much more out of the project if they are comfortable reading docs, flashing firmware, and following validation steps.

??? question "Is it open source?"

    Yes. The repo publishes firmware, software, hardware documentation, and supporting docs under the licenses described in [License and Support](LicenseAndSupport.md).

??? question "Is this a finished consumer product?"

    Not in the sense of a mass-market plug-and-play device with universal compatibility claims. The project is documented, real, and usable, but it is still framed honestly as a small-batch, artist-run instrument ecosystem with explicit validation and support boundaries.

??? question "What does the configurator do?"

    The browser configurator is the main setup and monitoring surface. It loads manifest/config data, stages edits safely, applies changes, and handles profile save/load workflows over the documented device contract.

??? question "What support is included?"

    The main support surface is the documentation itself: quickstarts, connectivity guidance, validation flow, troubleshooting, and license/support boundaries. See [License and Support](LicenseAndSupport.md) and [Pilot Run](PilotRun.md) for the practical limits.

??? question "Why would I choose this instead of a generic MIDI controller?"

    Because the value here is not just the control surface. It is the combination of hardware, firmware, configurator, bridge, and unusually explicit documentation. If that matters to you, read [Why MN42](WhyMN42.md).
