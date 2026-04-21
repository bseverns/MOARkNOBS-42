# Reproducibility Playbook

> No magic smoke. No shrug emoji. Just a cranky, repeatable build pipeline you can audit and rerun.

MOARkNOBS-42 publishes binaries *and* receipts. This guide walks through the exact commands we use to
rebuild a release, prove the toolchain version, and verify the artifacts with hashes. Follow it and you’ll
end up with the same `firmware.hex`, the same `hardware_reference.zip`, and a `manifest.json` that records every
step we took.

## TL;DR — clone, install, release

```bash
# 1. Grab the repo
git clone https://github.com/<your-fork-or-upstream>/MOARkNOBS-42.git
cd MOARkNOBS-42

# 2. Install PlatformIO in your current Python env
python -m pip install --upgrade pip
pip install platformio

# 3. Run the scripted release ritual (replace v0.0.0 with your tag)
FW_VERSION=v0.0.0 ./release.sh v0.0.0

# 4. Inspect the receipts
ls dist
jq '.' dist/manifest.json
```

Everything lands in `dist/`: the versioned firmware hex, hardware reference bundle, deterministic source export zip,
release verification summary, license docs, and a manifest that ties all of it to tool versions and git state.

## Step-by-step with commentary

### 1. Reset the playing field

The release script pins PlatformIO’s caches inside the repo (`.pio-home/` and `.pio-cache/`) so different
machines don’t pollute each other. If you’ve run a build before and want a squeaky-clean rerun, wipe them:

```bash
rm -rf .pio-home/ .pio-cache/ dist/
```

Fresh clones can skip that nuke—there’s nothing to clean yet.

### 2. Run the release lanes

`release.sh` is the single source of truth for local releases *and* the GitHub workflow. The firmware
metadata that lands in `GET_MANIFEST` is stringized in `firmware/include/version.h`, while
`firmware/scripts/version.py` emits the `-DFW_VERSION` / `-DGIT_SHA` build flags. That is why the local
invocation below exports `FW_VERSION=<tag>` before it runs the script.

The script does the following in order:

1. exports `PLATFORMIO_HOME_DIR`, `PLATFORMIO_PACKAGES_DIR`, etc. so every PlatformIO package lives inside
   the repo rather than your global cache;
2. runs `release_verify_hil.sh` and writes `.release_verification.json`:
   - `REQUIRE_HIL=1` runs `./test.sh --require-hil` and fails hard if `TEST_PORT` is missing;
   - default mode (`REQUIRE_HIL=0`) runs optional Unity HIL only when `TEST_PORT` is set, otherwise records
     an explicit skip;
3. cleans the Teensy build output (`pio run -t clean -e teensy40_main`);
4. rebuilds the firmware (`pio run -e teensy40_main`);
5. copies `mn42_<version>.hex` into `dist/`;
6. packs `hardware/fabrication/` into a deterministic `hardware_reference.zip` (timestamps frozen at 1980-01-01,
   permissions fixed at 0644, and entries sorted);
7. creates a deterministic source export zip;
8. copies license docs;
9. copies `release_verification.json` into `dist/`; and
10. calls `tools/generate_release_manifest.py` to capture hashes, git metadata, PlatformIO info, the
    verification summary, and the exact commands executed.

You trigger the whole dance with:

```bash
FW_VERSION=v0.0.0 ./release.sh v0.0.0
```

Swap `v0.0.0` for whatever tag you intend to cut.

### 3. Audit the manifest

`dist/manifest.json` is the reproducibility ledger. It contains:

- git commit + branch + dirtiness
- PlatformIO core/Python versions from `pio system info`
- the command strings for clean/build
- the firmware version string that was injected at build time
- verification truth from `dist/release_verification.json` (what ran vs what was skipped)
- SHA-256 hashes and byte sizes for the release artifacts
- the raw output of `pio pkg list` so you know which packages were installed

Peek at the interesting bits:

```bash
jq '{version, git, platformio: {system_info, home}, artifacts}' dist/manifest.json
```

Re-run the hash check and compare to the manifest:

```bash
sha256sum dist/mn42_v0.0.0.hex
sha256sum dist/hardware_reference.zip
```

Both digests should match the `artifacts` block in the manifest exactly.

### 4. Flash or ship with confidence

With the manifest and hashes in hand you can:

- flash the board locally using `pio run -d firmware -t upload -e teensy40_main`
- stash the `hardware_reference.zip` on the release page alongside the firmware so builders can send boards to
  fab without spelunking the repo
- archive `manifest.json` anywhere that expects a software BOM or build log

## How CI mirrors this

`.github/workflows/release.yml` runs `./release.sh` from the tagged source. That keeps the workflow on
the same scripted path as local releases rather than relying on a separate CI-only recipe. The workflow uploads:

- `mn42_<tag>.hex`
- `hardware_reference.zip`
- `mn42_<tag>_source.zip`
- `release_verification.json`
- `manifest.json`
- `SHA256SUMS.txt`
- the bundled license docs (`THIRD_PARTY_LICENSES.md` and the `LICENSES/` directory)

Important: the hosted CI release lane uses `REQUIRE_HIL=0` by default, so HIL may be skipped unless a
runner has `TEST_PORT` configured. That skip/execute state is recorded in `release_verification.json`
and mirrored into `manifest.json`.

Inspect the manifest in CI logs or download it straight from the release page to verify the run.

## Troubleshooting vibes

- **Missing PlatformIO** — make sure `platformio` resolves in your shell (`pio --version`). If it doesn’t,
  your Python install path probably isn’t on `PATH`.
- **Firmware reports `0.0.0` after a release build** — you probably forgot to export `FW_VERSION=<tag>`
  before running `./release.sh`. Re-run with the version env var set so the build helper emits the right flag.
- **Verification is skipped** — check `dist/release_verification.json` and `manifest.tests`.
  If you need hard hardware proof, rerun with `REQUIRE_HIL=1 TEST_PORT=<device>`.
- **Hash mismatch** — ensure you didn’t edit artifacts after the fact. Re-run `./release.sh` to regenerate
  clean copies.

Document the weirdness if you hit new edge cases. Reproducibility only gets sharper when we log the dirt.
