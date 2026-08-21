# MOARkNOBS-42 Wiki

> **Archived:** This wiki is a historical snapshot and is not maintained. Start with the canonical
> [repository README](https://github.com/bseverns/MOARkNOBS-42/blob/main/README.md) or
> [documentation site](https://bseverns.github.io/MOARkNOBS-42/).

MOARkNOBS-42 is an open hardware, firmware, and software instrument built around
a Teensy 4.0 reactive MIDI/OSC controller.

The links below are retained for old bookmarks. Current technical detail remains in the repository source tree.
Canonical source: `README.md`

![Connectivity decision map showing the direct browser configurator path beside the bridge path used for OSC, virtual MIDI, and DAW integration.](assets/workflows/connectivity-decision-overview.png)

## I have a finished instrument

Start here when you want to use the machine rather than build its software.

- [Start using the instrument](Getting-Started.md)
- [Make the first playable patch](Playable-Walkthrough.md)
- [Configure it in the browser](WebSerial-App.md)
- [Save and recall a setup](WebSerial-App.md#save-recall-and-back-up)
- [Connect it to a DAW or OSC host](OSC-Bridge.md)
- [Find a symptom and recover](Troubleshooting.md)

## I am building one

Start here for fabrication boundaries, flashing, bring-up, and physical checks.

- [Build and inspect the hardware](Hardware.md)
- [Developer setup and firmware upload](Developer-Setup.md)
- [Test and troubleshoot](Testing.md)

The repository is currently a hardware-test package. Read the fabrication and
power-boundary notes before treating it as a production-ready build package.

## I am developing the project

Start here for the contributor toolchain, architecture, tests, and releases.

- [Developer setup](Developer-Setup.md)
- [Develop the firmware](Firmware.md)
- [Understand the system](System-Architecture.md)
- [Run the test layers](Testing.md)
- [Prepare a release](Release-Process.md)
- [Read project history](History-and-Roadmap.md)

## Canonical source docs

- Root overview: `README.md`
- Docs index: `docs/index.md`
- Persistence contract: `docs/reference/PersistenceContract.md`
- Firmware details: `firmware/README.md`
- Bridge details: `bridge/README.md`
- App details: `App/README.md`
