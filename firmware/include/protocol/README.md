# Protocol Header Reading Path

This folder holds the protocol-side declarations for the host/configurator
stack.

For the source-level machine walkthrough, pair this page with
[../../docs/firmware/ProtocolStackReadingPath.md](../../../docs/firmware/ProtocolStackReadingPath.md).

The fastest header-first route is:

1. [../Protocol.h](../Protocol.h)
2. [ProtocolDispatch.h](ProtocolDispatch.h)
3. [ConfigBulkTransport.h](ConfigBulkTransport.h)
4. [ConfigJsonApply.h](ConfigJsonApply.h)
5. [ConfigApplyDigest.h](ConfigApplyDigest.h)
6. [ProtocolSimpleHandlers.h](ProtocolSimpleHandlers.h)
7. [ProtocolLiveControlHandlers.h](ProtocolLiveControlHandlers.h)
8. [ManifestReport.h](ManifestReport.h)
9. [ProtocolErrors.h](ProtocolErrors.h)
10. the profile / scene / SysEx helper headers as needed

Use this folder together with
[../../src/protocol/README.md](../../src/protocol/README.md) if you want to
trace the protocol machine declaration-first and then execution-first.
