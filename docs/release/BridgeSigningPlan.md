# Bridge Signing Plan

This is a signing-ready plan, not a claim that the bridge is currently shipped as a signed public installer.

## Current state

- CI builds unsigned per-target bridge artifacts.
- Those artifacts are tagged as `signingStatus: "unsigned-ci-artifact"` and include a nested `signing` metadata object in the bridge artifact manifest.
- The release workflow now treats those bundles as internal/operator evidence, not polished consumer installers.

## Intended future path

1. Add per-platform signing credentials or wrapper commands in CI secrets.
2. Enable `REQUIRE_BRIDGE_SIGNING=1` for packaging jobs that must fail closed if signing does not occur.
3. For macOS, add notarization on top of code signing.
4. Ship a small launcher/install wrapper only after the signed binary path is stable.
5. Keep the CLI help path and the browser-console server path documented even after a signed wrapper exists.

## Per-platform expectations

### macOS

- Code-sign the packaged binary with a Developer ID identity.
- Notarize the submitted artifact.
- Staple/notary-verify before publishing a public-facing bundle.

### Windows

- Authenticode-sign the packaged executable.
- Decide whether SmartScreen reputation or an installer wrapper is required before claiming a public release path.

### Linux

- Signing expectations depend on the eventual distribution model.
- For now the unsigned CI artifact plus checksum is the documented path.

## Release metadata expectations

Every packaged bridge target should continue to publish:

- checksum file
- per-target `README.txt`
- bundled third-party license notices
- `bridge_artifact_manifest.json`

The signing path should add:

- `signing.status`
- `signing.identity` when an identity is available
- `signing.notarizationStatus` where relevant
- `signing.note` with the support boundary for signed or unsigned artifacts
- a signed-vs-unsigned support note in the release text

## Non-goals right now

- No signing credentials are required for current development or CI.
- No public-installer claim should be added until signing, notarization, and smoke validation are all proven.
