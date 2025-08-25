#!/usr/bin/env bash
# Release helper: builds firmware and bundles license intel.
# Tagging a version on GitHub kicks off `.github/workflows/release.yml`,
# which rebuilds the hex and drops a sys report on the release page.
# This script sticks around for local smoke-tests and offline rituals.
#
# Keep this script honest: the distro **must** ship the source license texts from
# `firmware/LICENSES/` alongside `THIRD_PARTY_LICENSES.md`. Skip that and you're
# basically inviting a cease-and-desist rave.
set -euo pipefail  # bail loudly on undefined vars, failed pipes, or any error

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <version>" >&2
  exit 1
fi  # demand one arg: the version tag, so files get named right

VERSION="$1"
OUTPUT_DIR="dist"  # stash built bits here
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"  # absolute path to repo root

mkdir -p "$ROOT_DIR/$OUTPUT_DIR"  # make sure dist exists before we fill it

pushd "$ROOT_DIR/firmware" >/dev/null  # drop into firmware land
# Kick the tires before we torch the build server.
echo "Running Unity tests..."  # run unit tests to prove firmware still behaves
pio test -e teensy40_unity || {
  echo "Tests failed. Refusing to ship busted bits." >&2
  exit 1
}

# If we made it here, the rig's cool. Build the main firmware.
pio run -e teensy40_main  # compile the real deal
cp .pio/build/teensy40_main/firmware.hex "$ROOT_DIR/$OUTPUT_DIR/mn42_${VERSION}.hex"  # stash hex with version
popd >/dev/null  # back to repo root

# Hoard the legal paperwork.
cp "$ROOT_DIR/THIRD_PARTY_LICENSES.md" "$ROOT_DIR/$OUTPUT_DIR/"  # keep license roster
cp -r "$ROOT_DIR/firmware/LICENSES" "$ROOT_DIR/$OUTPUT_DIR/"     # ship source license files

echo "Release artifacts ready in $OUTPUT_DIR/"
ls "$ROOT_DIR/$OUTPUT_DIR"
echo "Push a tag and let CI upload the goods to GitHub releases."
