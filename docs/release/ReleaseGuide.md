# Release Guide

Need to cut a proper drop? Start with [Release Story](ReleaseStory.md), then treat this file as the operator checklist and [Reproducibility](Reproducibility.md) as the canonical artifact recipe.

```mermaid
flowchart LR
  A[Build + tests] --> B[Update notes]
  B --> C[Stamp version]
  C --> D[Tag commit]
  D --> E[release.sh]
  E --> F[Artifacts + manifest]
  F --> G[Upload / publish]
```

1. **Run the gauntlet** – build (`pio run -d firmware -e teensy40_main`) and test (`pio test -d firmware -e teensy40_unity -vvv`, `npm --prefix bridge test`, `npm --prefix App test`). For a beta/public candidate, run `./release_verify_hil.sh`; it always runs the bridge and app suites, and it adds Unity/full-stack HIL when `TEST_PORT` or an auto-detected board is available. Use `REQUIRE_HIL=1` when hardware evidence is mandatory.
2. **Refresh the human-facing release notes** – update [CHANGELOG.md](https://github.com/bseverns/MOARkNOBS-42/blob/main/CHANGELOG.md) and the relevant entry in [HISTORY.md](../project/HISTORY.md) so both the terse delta log and the longer narrative are current.
3. **Confirm version stamping** – firmware version strings come from `FW_VERSION`/`GIT_SHA`, which are stringized in [firmware/include/version.h](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/include/version.h) and injected by [firmware/scripts/version.py](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/scripts/version.py). For local release builds, export the intended tag before invoking the release script:
   ```bash
   FW_VERSION=vX.Y.Z ./release.sh vX.Y.Z
   ```
   - `release.sh` now runs two explicit lanes:
     - `release_verify_hil.sh` (hardware verification; enforce with `REQUIRE_HIL=1`)
     - `release_build.sh` (deterministic artifact generation)
   - `release_verify_hil.sh` always emits `release_verification.json` so artifacts explicitly show what verification was executed versus skipped.
   - `release_build.sh` refuses tracked source changes before packaging so `dist/manifest.json` records a clean source commit.
4. **Commit it** – lock in doc updates and any release prep changes with a commit.
5. **Tag it loud** – `git tag -a vX.Y.Z -m "vX.Y.Z"` to mark the moment.
6. **Push the tag** – `git push origin vX.Y.Z` kicks CI into `.github/workflows/release.yml`.
7. **Draft the release** – on GitHub, create a new release from that tag, drop the human-written notes, and publish.
   - The release workflow builds firmware artifacts and unsigned bridge packages (`macOS x64 + arm64`, `Linux x64`, `Windows x64`) and always stores them as workflow artifacts.
   - Release packaging is blocked unless bridge tests, app tests, and `teensy40_main` firmware build pass in the release workflow.
   - Assets are uploaded to GitHub Releases only when a release for that tag already exists. If not, rerun the workflow (or run manual dispatch) after creating the release.
   - Read `release_verification.json` before publishing notes: default hosted runners use optional HIL mode unless you provide a hardware port.
   - Bridge uploads include per-target checksums and bridge third-party license payloads.
8. **Celebrate or debug** – if CI faceplants, fix it and retag. If it works, cue the lights.

## Bridge packaging track

Unsigned bridge packaging now runs automatically in the release workflow. The remaining manual track is signing and installer-grade polish:

1. Follow [`BridgePackaging.md`](BridgePackaging.md) phases for the current release target.
2. Review the generated bridge artifacts and checksums from CI.
3. For beta/public artifacts, package with `REQUIRE_BRIDGE_SIGNING=1` and provide either platform credentials (`APPLE_CODESIGN_IDENTITY`, `APPLE_NOTARY_PROFILE`) or signing hooks (`BRIDGE_SIGNING_COMMAND`, `BRIDGE_NOTARIZE_COMMAND`).
4. Attach/verify signed bridge artifacts alongside firmware files on the GitHub release.
5. Verify docs match the shipped UX (`docs/OSCBridge.md`, `docs/BridgeForPerformers.md`, `bridge/README.md`).
6. Complete the artifact checklist template: [`release/bridge-artifacts-checklist.md`](bridge-artifacts-checklist.md).
