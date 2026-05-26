# Protocol Header Reading Path

This folder holds the protocol-side declarations for the host/configurator
stack.

For the source-level machine walkthrough, pair this page with
[../../docs/firmware/ProtocolStackReadingPath.md](../../../docs/firmware/ProtocolStackReadingPath.md).

The fastest header-first route is:

1. [../Protocol.h](../Protocol.h)
2. [ProtocolDispatch.h](ProtocolDispatch.h)
3. [ConfigJsonApply.h](ConfigJsonApply.h)
4. [ManifestReport.h](ManifestReport.h)
5. [ProtocolErrors.h](ProtocolErrors.h)
6. [ProtocolSimpleHandlers.h](ProtocolSimpleHandlers.h)
7. the profile / scene / SysEx helper headers as needed

Use this folder together with
[../../src/protocol/README.md](../../src/protocol/README.md) if you want to
trace the protocol machine declaration-first and then execution-first.
