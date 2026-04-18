# Release Guide

Need to cut a proper drop? Start with [Release Story](ReleaseStory.md), then treat this file as the operator checklist and [Reproducibility](Reproducibility.md) as the canonical artifact recipe.

1. **Run the gauntlet** – build (`pio -d firmware run -e teensy40_main`) and test (`pio -d firmware test -e teensy40_unity -vvv`, `npm --prefix bridge test`). Don't ship broken noise.
2. **Refresh the human-facing release notes** – update [CHANGELOG.md](https://github.com/bseverns/benzknober/blob/main/CHANGELOG.md) and the relevant entry in [HISTORY.md](../project/HISTORY.md) so both the terse delta log and the longer narrative are current.
3. **Confirm version stamping** – firmware version strings come from `FW_VERSION`/`GIT_SHA`, which are stringized in [firmware/include/version.h](https://github.com/bseverns/benzknober/blob/main/firmware/include/version.h) and injected by [firmware/scripts/version.py](https://github.com/bseverns/benzknober/blob/main/firmware/scripts/version.py). For local release builds, export the intended tag before invoking the release script:
   ```bash
   FW_VERSION=vX.Y.Z ./release.sh vX.Y.Z
   ```
   - `release.sh` now runs two explicit lanes:
     - `release_verify_hil.sh` (hardware verification; enforce with `REQUIRE_HIL=1`)
     - `release_build.sh` (deterministic artifact generation)
   - `release_verify_hil.sh` always emits `release_verification.json` so artifacts explicitly show what verification was executed versus skipped.
4. **Commit it** – lock in doc updates and any release prep changes with a commit.
5. **Tag it loud** – `git tag -a vX.Y.Z -m "vX.Y.Z"` to mark the moment.
6. **Push the tag** – `git push origin vX.Y.Z` kicks CI into gear and runs the same release script described in [Reproducibility](Reproducibility.md).
7. **Draft the release** – on GitHub, create a new release from that tag, drop the human-written notes, and publish.
   - CI uploads the firmware hex, deterministic fabrication zip, source export tarball, `release_verification.json`, manifest, checksums, and license payloads.
   - Read `release_verification.json` before publishing notes: default hosted runners use optional HIL mode unless you provide a hardware port.
   - If you ran bridge packaging, attach those assets + checksums too.
8. **Celebrate or debug** – if CI faceplants, fix it and retag. If it works, cue the lights.

## Bridge packaging track (optional but recommended)

If you are shipping to non-CLI users, run the bridge packaging lane too:

1. Follow [`BridgePackaging.md`](BridgePackaging.md) phases for the current release target.
2. Build platform artifacts and run bridge smoke tests on each OS.
3. Sign binaries/installers where applicable.
4. Attach bridge artifacts + checksums to the GitHub release alongside firmware files.
5. Verify docs match the shipped UX (`docs/OSCBridge.md`, `docs/BridgeForPerformers.md`, `bridge/README.md`).
6. Complete the artifact checklist template: [`release/bridge-artifacts-checklist.md`](bridge-artifacts-checklist.md).
