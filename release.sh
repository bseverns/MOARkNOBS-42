#!/usr/bin/env bash
# Release helper: builds firmware and bundles license intel.
# Tagging a version on GitHub kicks off `.github/workflows/release.yml`,
# which rebuilds the hex and drops a sys report on the release page.
# This script sticks around for local smoke-tests and offline rituals.
#
# Keep this script honest: the distro **must** ship the source license texts from
# `firmware/LICENSES/` alongside `THIRD_PARTY_LICENSES.md`. Skip that and you're
# basically inviting a cease-and-desist rave.
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <version>" >&2
  exit 1
fi

VERSION="$1"
OUTPUT_DIR="dist"
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

mkdir -p "$ROOT_DIR/$OUTPUT_DIR"

pushd "$ROOT_DIR/firmware" >/dev/null
# Kick the tires before we torch the build server.
echo "Running Unity tests..."
pio test -e teensy40_unity || {
  echo "Tests failed. Refusing to ship busted bits." >&2
  exit 1
}

# If we made it here, the rig's cool. Build the main firmware.
pio run -e teensy40_main
cp .pio/build/teensy40_main/firmware.hex "$ROOT_DIR/$OUTPUT_DIR/mn42_${VERSION}.hex"
popd >/dev/null

# Hoard the legal paperwork.
cp "$ROOT_DIR/THIRD_PARTY_LICENSES.md" "$ROOT_DIR/$OUTPUT_DIR/"
cp -r "$ROOT_DIR/firmware/LICENSES" "$ROOT_DIR/$OUTPUT_DIR/"

echo "Release artifacts ready in $OUTPUT_DIR/"
ls "$ROOT_DIR/$OUTPUT_DIR"
echo "Push a tag and let CI upload the goods to GitHub releases."
