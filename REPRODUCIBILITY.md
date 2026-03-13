# Reproducibility Playbook

> No magic smoke. No shrug emoji. Just a cranky, repeatable build pipeline you can audit and rerun.

MOARkNOBS-42 publishes binaries *and* receipts. This guide walks through the exact commands we use to
rebuild a release, prove the toolchain version, and verify the artifacts with hashes. Follow it and you’ll
end up with the same `firmware.hex`, the same `fabrication.zip`, and a `manifest.json` that records every
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

Everything lands in `dist/`: the versioned firmware hex, the fabrication bundle, license docs, and the
manifest that ties them to tool versions and git state.

## Step-by-step with commentary

### 1. Reset the playing field

The release script pins PlatformIO’s caches inside the repo (`.pio-home/` and `.pio-cache/`) so different
machines don’t pollute each other. If you’ve run a build before and want a squeaky-clean rerun, wipe them:

```bash
rm -rf .pio-home/ .pio-cache/ dist/
```

Fresh clones can skip that nuke—there’s nothing to clean yet.

### 2. Run the deterministic build script

`release.sh` is the single source of truth for local releases *and* the GitHub workflow. The firmware
metadata that lands in `GET_MANIFEST` is stringized in `firmware/include/version.h`, while
`firmware/scripts/version.py` emits the `-DFW_VERSION` / `-DGIT_SHA` build flags. That is why the local
invocation below exports `FW_VERSION=<tag>` before it runs the script.

The script does the following in order:

1. exports `PLATFORMIO_HOME_DIR`, `PLATFORMIO_PACKAGES_DIR`, etc. so every PlatformIO package lives inside
   the repo rather than your global cache;
2. runs the Unity test suite (`pio test -e teensy40_unity`), refusing to continue on failure;
3. cleans the Teensy build output (`pio run -t clean -e teensy40_main`);
4. rebuilds the firmware (`pio run -e teensy40_main`);
5. copies `mn42_<version>.hex` into `dist/`;
6. packs `hardware/fabrication/` into a deterministic `fabrication.zip` (timestamps frozen at 1980-01-01,
   permissions fixed at 0644, and entries sorted);
7. copies the license docs; and
8. calls `tools/generate_release_manifest.py` to capture hashes, git metadata, PlatformIO info, and the
   exact commands executed.

You trigger the whole dance with:

```bash
FW_VERSION=v0.0.0 ./release.sh v0.0.0
```

Swap `v0.0.0` for whatever tag you intend to cut.

### 3. Audit the manifest

`dist/manifest.json` is the reproducibility ledger. It contains:

- git commit + branch + dirtiness
- PlatformIO core/Python versions from `pio system info`
- the command strings for tests, clean, and build
- the firmware version string that was injected at build time
- SHA-256 hashes and byte sizes for `mn42_<version>.hex` and `fabrication.zip`
- the raw output of `pio pkg list` so you know which packages were installed

Peek at the interesting bits:

```bash
jq '{version, git, platformio: {system_info, home}, artifacts}' dist/manifest.json
```

Re-run the hash check and compare to the manifest:

```bash
sha256sum dist/mn42_v0.0.0.hex
sha256sum dist/fabrication.zip
```

Both digests should match the `artifacts` block in the manifest exactly.

### 4. Flash or ship with confidence

With the manifest and hashes in hand you can:

- flash the board locally using `pio -d firmware run -t upload -e teensy40_main`
- stash the `fabrication.zip` on the release page alongside the firmware so builders can send boards to
  fab without spelunking the repo
- archive `manifest.json` anywhere that expects a software BOM or build log

## How CI mirrors this

`.github/workflows/release.yml` now runs `./release.sh` from the tagged source. That keeps the workflow on
the same scripted path as local releases rather than relying on a separate CI-only recipe. The workflow uploads:

- `mn42_<tag>.hex`
- `fabrication.zip`
- `manifest.json`
- the bundled license docs (`THIRD_PARTY_LICENSES.md` and the `LICENSES/` directory)

Inspect the manifest in CI logs or download it straight from the release page to verify the run.

## Troubleshooting vibes

- **Missing PlatformIO** — make sure `platformio` resolves in your shell (`pio --version`). If it doesn’t,
  your Python install path probably isn’t on `PATH`.
- **Firmware reports `0.0.0` after a release build** — you probably forgot to export `FW_VERSION=<tag>`
  before running `./release.sh`. Re-run with the version env var set so the build helper emits the right flag.
- **Tests fail** — the script bails immediately so you don’t accidentally publish busted firmware. Fix the
  failure (or file an issue) before rerunning.
- **Hash mismatch** — ensure you didn’t edit artifacts after the fact. Re-run `./release.sh` to regenerate
  clean copies.

Document the weirdness if you hit new edge cases. Reproducibility only gets sharper when we log the dirt.
