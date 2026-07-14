# Firmware Main Reading Path

This is an orientation doc. For document tie-break rules, see
[Documentation Truth Map](../reference/DocumentationTruthMap.md).

If someone wants to learn the firmware by reading code in order, start at
[`firmware/src/firmware_main.cpp`](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/src/firmware_main.cpp).
That file is intentionally small enough to show the machine's top-level
composition before the deeper modules take over.

## Why Start Here

`firmware_main.cpp` answers the first four questions a learner usually has:

1. Which boot modes exist?
2. What gets initialized first?
3. What stays alive for the whole runtime?
4. What actually runs on every loop tick?

The file is not where the detailed behavior lives. It is where the machine's
major stacks are named in one place.

## Read The Headers In This Order

Follow the include list in `firmware_main.cpp` from top to bottom.

### 1. `BootMode.h`

Question answered: which personality should this boot become?

- `StandaloneRuntime` means "be the instrument."
- `UsbConfigurator` means "be the host-facing configuration lane."

This is the first fork in the machine.

### 2. `DiagnosticRecord.h`

Question answered: what boot and configuration diagnostics survive for later inspection?

This is the persistent record for reset causes, boot mode, configuration-apply
results, and runtime health markers.

### 3. `FirmwareState.h`

Question answered: which long-lived objects exist once the machine is alive?

This is the runtime inventory:

- `configManager`
- `midiHandler`
- `potentiometerManager`
- `buttonManager`
- `displayManager`
- `ledManager`
- `envelopeFollowers`
- `lfoManager`
- scheduler and UI/runtime flags

Read this header as the cast list for the running firmware.

After the header, read
[`firmware/src/SystemState.cpp`](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/src/SystemState.cpp) to see
where that cast is actually instantiated and wired together.

### 4. `Globals.h`

Question answered: which physical and persisted facts shape the machine?

This is the firmware's "world contract":

- hardware counts like `NUM_POTS`, `NUM_SLOTS`, `NUM_ENVELOPES`
- pin and timing defaults in `HardwareConfig`
- EEPROM layout and schema version
- cross-cutting state such as `g_brownoutCount`, `g_tappedBPM`, and
  `webSerialStreaming`

Read this header as the machine's constants, memory map, and shared scalar
state.

### 5. `Protocol.h`

Question answered: how does the host talk to the firmware?

This is the configurator/debug lane:

- startup handshake
- command parsing
- `SET_ALL` / `GET_*` behavior
- host-facing serialization helpers

When the boot mode is `UsbConfigurator`, `firmware_main.cpp` mostly hands
control to this stack.

After the header, read the protocol machine in this order:

1. [`firmware/src/protocol/Protocol.cpp`](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/src/protocol/Protocol.cpp)
2. [`firmware/include/protocol/ProtocolDispatch.h`](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/include/protocol/ProtocolDispatch.h)
3. [`firmware/src/protocol/ProtocolDispatch.cpp`](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/src/protocol/ProtocolDispatch.cpp)
4. [`firmware/include/protocol/ConfigJsonApply.h`](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/include/protocol/ConfigJsonApply.h)
5. [`firmware/src/protocol/ConfigJsonApply.cpp`](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/src/protocol/ConfigJsonApply.cpp)

That preserves the real flow:

- boot/startup protocol bring-up
- whole-line command routing
- bulk config apply
- then the specialized handler families

For the fuller protocol-machine walkthrough, continue into
[Protocol Stack Reading Path](ProtocolStackReadingPath.md).

### 6. `CommandQueue.h`

Question answered: how do serial bytes become whole commands?

This is a small but important seam between transport noise and protocol logic.

### 7. `Log.h`

Question answered: how does the firmware speak back out?

Use this to understand where structured responses, debug lines, and boot/status
messages leave the board.

### 8. `Modes.h`

Question answered: how is stored musical state restored into live runtime state?

This header is the bridge between persistence and behavior:

- profile snapshot capture/apply
- envelope follower settings hydration
- LFO configuration
- startup mode restoration

After the header, read
[`firmware/src/modes/Modes.cpp`](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/src/modes/Modes.cpp) in this
order:

1. translation helpers between stored and live state
2. profile snapshot encode/decode helpers
3. profile capture/apply
4. boot-time reconstruction of modulation/runtime state
5. shared label/cache maintenance

### 9. `UI.h`

Question answered: what is the on-device operator surface?

This is the OLED/button/control-overlay lane, not the browser configurator
lane.

### 10. `Runtime.h`

Question answered: what work repeats while the instrument is running?

This is the hot path:

- MIDI service
- envelope follower processing
- internal clock
- diagnostics
- runtime initialization

If `firmware_main.cpp` is the conductor, `Runtime.h` is the score the band
plays every frame.

After the header, read
[`firmware/src/Runtime.cpp`](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/src/Runtime.cpp) as:

1. private queues and timing helpers
2. runtime bring-up
3. high-frequency service lanes
4. mid-tier musical processing
5. diagnostics and health reporting

### 11. `Utility.h`

Question answered: which low-level helpers and schedulers support everything
above?

This is shared infrastructure:

- lightweight schedulers
- mapping/scaling helpers
- reboot helpers
- bulk-config assembler

It is broad by design, so it should usually be read last in this path.

## Read `setup()` As Two Different Machines

`setup()` is easier to understand if you read it as two possible boot stories.

### USB Configurator Boot

1. decide boot mode
2. initialize protocol
3. record boot diagnostics
4. load hardware configuration
5. restore the active profile into runtime state
6. emit configurator boot marker and stop before runtime/UI/scheduler bring-up

This path exists so host tools can get a quiet, controlled configuration lane.

### Standalone Runtime Boot

1. decide boot mode
2. initialize protocol and boot diagnostics
3. load hardware configuration
4. restore stored musical/config state via `initializeModes()`
5. build the on-device UI
6. arm the runtime schedulers

This path exists so the board behaves as an instrument even with no browser or
bridge attached.

## Read `loop()` As A Priority Split

`loop()` also has two personalities.

### Configurator Loop

- poll serial input
- assemble commands
- dispatch protocol handlers

No scheduler-heavy runtime work happens here.

### Standalone Runtime Loop

- run the high/mid/low schedulers
- service deferred profile load/save requests
- monitor system load

That means the true runtime behavior is mostly distributed into scheduled tasks,
not written inline in `loop()`.

## Best Next Files After The Headers

After reading the headers above, use this order:

1. [`firmware/src/firmware_main.cpp`](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/src/firmware_main.cpp)
2. [`firmware/src/BootMode.cpp`](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/src/BootMode.cpp)
3. [`firmware/src/Runtime.cpp`](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/src/Runtime.cpp)
4. [`firmware/src/modes/Modes.cpp`](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/src/modes/Modes.cpp)
5. [`firmware/src/protocol/Protocol.cpp`](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/src/protocol/Protocol.cpp)
6. [`firmware/src/Globals.cpp`](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/src/Globals.cpp)

That order preserves the top-level story:

- choose the machine
- initialize the machine
- run the machine
- configure the machine
- inspect its shared world

## Teaching Intent

If the repo is being used as a reference implementation, the important thing is
not that every subsystem is tiny. The important thing is that a reader can keep
answering, in order:

1. What concept am I looking at?
2. Which layer owns it?
3. Is it boot-time, runtime, host-facing, or hardware-facing?
4. Which file should I read next?

This reading path is meant to preserve that traceability even as the firmware
grows.
