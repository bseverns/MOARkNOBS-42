#!/usr/bin/env bash
# Release helper: builds firmware and bundles license intel.
# Tagging a version on GitHub kicks off `.github/workflows/release.yml`,
# which rebuilds the hex and drops the manifest/fabrication bundle on the release page.
# This script sticks around for local smoke-tests and offline rituals.
#
# Keep this script honest: the distro **must** ship the source license texts from
# `firmware/LICENSES/` alongside `THIRD_PARTY_LICENSES.md`. Skip that and you're
# basically inviting a cease-and-desist rave.
set -euo pipefail  # bail loudly on undefined vars, failed pipes, or any error
IFS=$'\n\t'
umask 022  # deterministic file perms so hashes don't wobble

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <version>" >&2
  exit 1
fi  # demand one arg: the version tag, so files get named right

VERSION="$1"
OUTPUT_DIR="dist"  # stash built bits here
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"  # absolute path to repo root
BUILD_ENV="teensy40_main"
UNITY_ENV="teensy40_unity"
FIRMWARE_NAME="mn42_${VERSION}.hex"
FABRICATION_NAME="fabrication.zip"
MANIFEST_NAME="manifest.json"
PROJECT_DIR="$ROOT_DIR/firmware"
FABRICATION_DIR="$ROOT_DIR/hardware/fabrication"

# Pin PlatformIO state inside the repo so rebuilds can be reproduced on any host.
PIO_HOME="$ROOT_DIR/.pio-home"
PIO_CACHE="$ROOT_DIR/.pio-cache"
export PLATFORMIO_HOME_DIR="$PIO_HOME"
export PLATFORMIO_CORE_DIR="$PIO_HOME"
export PLATFORMIO_GLOBALLIB_DIR="$PIO_HOME/lib"
export PLATFORMIO_PLATFORM_DIR="$PIO_HOME/platforms"
export PLATFORMIO_PACKAGES_DIR="$PIO_HOME/packages"
export PLATFORMIO_CACHE_DIR="$PIO_CACHE"
export PLATFORMIO_NO_ANALYTICS=1
export PLATFORMIO_SETTING_ENABLE_PROMPTS=0

mkdir -p "$ROOT_DIR/$OUTPUT_DIR" "$PIO_HOME" "$PIO_CACHE"

pushd "$PROJECT_DIR" >/dev/null  # drop into firmware land

UNITY_CMD=(pio test -e "$UNITY_ENV")
echo "Running Unity tests..."  # run unit tests to prove firmware still behaves
"${UNITY_CMD[@]}" || {
  echo "Tests failed. Refusing to ship busted bits." >&2
  exit 1
}

CLEAN_CMD=(pio run -t clean -e "$BUILD_ENV")
echo "Scrubbing previous build outputs..."
"${CLEAN_CMD[@]}"

BUILD_CMD=(pio run -e "$BUILD_ENV")
echo "Building firmware..."
"${BUILD_CMD[@]}"

cp ".pio/build/${BUILD_ENV}/firmware.hex" "$ROOT_DIR/$OUTPUT_DIR/$FIRMWARE_NAME"  # stash hex with version
popd >/dev/null  # back to repo root

# Bake fabrication.zip deterministically so SHA-256 stays fixed across hosts.
if [ ! -d "$FABRICATION_DIR" ]; then
  echo "Missing fabrication source at $FABRICATION_DIR" >&2
  exit 1
fi

python3 - "$FABRICATION_DIR" "$ROOT_DIR/$OUTPUT_DIR/$FABRICATION_NAME" <<'PY'
import pathlib
import sys
import zipfile

src = pathlib.Path(sys.argv[1]).resolve()
dst = pathlib.Path(sys.argv[2]).resolve()

if dst.exists():
    dst.unlink()

with zipfile.ZipFile(dst, "w", compression=zipfile.ZIP_DEFLATED) as zf:
    for path in sorted(src.rglob("*")):
        if not path.is_file():
            continue
        arcname = path.relative_to(src).as_posix()
        info = zipfile.ZipInfo(arcname)
        info.date_time = (1980, 1, 1, 0, 0, 0)  # freeze timestamps for deterministic zips
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o100644 << 16  # regular file with 0644 perms
        with path.open("rb") as fh:
            zf.writestr(info, fh.read())
PY

# Hoard the legal paperwork.
cp "$ROOT_DIR/THIRD_PARTY_LICENSES.md" "$ROOT_DIR/$OUTPUT_DIR/"  # keep license roster
rm -rf "$ROOT_DIR/$OUTPUT_DIR/LICENSES"
cp -r "$ROOT_DIR/firmware/LICENSES" "$ROOT_DIR/$OUTPUT_DIR/"     # ship source license files

# Spill a build manifest with hashes, commands, and toolchain provenance.
UNITY_CMD_STR=$(printf '%q ' "${UNITY_CMD[@]}")
CLEAN_CMD_STR=$(printf '%q ' "${CLEAN_CMD[@]}")
BUILD_CMD_STR=$(printf '%q ' "${BUILD_CMD[@]}")

python3 "$ROOT_DIR/tools/generate_release_manifest.py" \
  --version "$VERSION" \
  --root "$ROOT_DIR" \
  --project "$PROJECT_DIR" \
  --build-env "$BUILD_ENV" \
  --output "$ROOT_DIR/$OUTPUT_DIR/$MANIFEST_NAME" \
  --firmware "$ROOT_DIR/$OUTPUT_DIR/$FIRMWARE_NAME" \
  --fabrication "$ROOT_DIR/$OUTPUT_DIR/$FABRICATION_NAME" \
  --pio-home "$PIO_HOME" \
  --test "$UNITY_ENV=passed" \
  --step "tests=$UNITY_CMD_STR" \
  --step "clean=$CLEAN_CMD_STR" \
  --step "build=$BUILD_CMD_STR"

echo "Release artifacts ready in $OUTPUT_DIR/"
ls "$ROOT_DIR/$OUTPUT_DIR"
echo "Push a tag and let CI upload the goods to GitHub releases."
