# Protocol Source Reading Path

This folder holds the firmware's host/configurator execution stack.

If you are learning the whole firmware from the top, start with
[../../docs/firmware/FirmwareMainReadingPath.md](../../../docs/firmware/FirmwareMainReadingPath.md)
and follow the `Protocol.h` step from there.

If you want the protocol stack explained as a machine rather than just as a
file order, read
[../../docs/firmware/ProtocolStackReadingPath.md](../../../docs/firmware/ProtocolStackReadingPath.md)
alongside this page.

If you are already standing inside `src/protocol/`, read these files in order:

1. [Protocol.cpp](Protocol.cpp)
2. [../../include/protocol/ProtocolDispatch.h](../../include/protocol/ProtocolDispatch.h)
3. [ProtocolDispatch.cpp](ProtocolDispatch.cpp)
4. [ConfigBulkTransport.cpp](ConfigBulkTransport.cpp)
5. [ConfigJsonApply.cpp](ConfigJsonApply.cpp)
6. [MidiInputConfigCodec.cpp](MidiInputConfigCodec.cpp)
7. [ConfigApplyDigest.cpp](ConfigApplyDigest.cpp)
8. [ProtocolSimpleHandlers.cpp](ProtocolSimpleHandlers.cpp)
9. [ChunkedReadTransport.cpp](ChunkedReadTransport.cpp)
10. [ModMatrixReport.cpp](ModMatrixReport.cpp)
11. [ProtocolLiveControlHandlers.cpp](ProtocolLiveControlHandlers.cpp)
12. [ProtocolErrors.cpp](ProtocolErrors.cpp)
13. [ManifestReport.cpp](ManifestReport.cpp)
14. [ProfileCommands.cpp](ProfileCommands.cpp)
15. [ProfileSetHandler.cpp](ProfileSetHandler.cpp)
16. [ProfileMacroHandlers.cpp](ProfileMacroHandlers.cpp)
17. [SceneCommands.cpp](SceneCommands.cpp)
18. [SceneStorage.cpp](SceneStorage.cpp)

That order preserves the machine shape:

- top-level protocol execution
- command routing
- bulk transport, transactional apply, MIDI-input config codec, and applied-state digest
- direct reads, bounded read transport, modulation reports, and live-control writes
- error formatting
- manifest/report emission
- profile slot lifecycle
- structured profile patching
- profile, macro, and arp command wrappers
- scene JSON front door
- whole-machine scene/macro snapshot storage

`Protocol.cpp` should answer "how does a host line enter the firmware and where
does it go next?" The later files answer what each handler family actually
does.
