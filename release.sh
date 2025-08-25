#!/usr/bin/env bash
# Release helper: builds firmware and bundles license intel.
# Tagging a version on GitHub kicks off `.github/workflows/release.yml`,
# which rebuilds the hex and drops a sys report on the release page.
# This script sticks around for local smoke-tests and offline rituals.
#
# Keep this script honest: the distro **must** ship the source license texts from
# `firmware/LICENSES/` alongside `THIRD_PARTY_LICENSES.md`. Skip that and you're
# basically inviting a cease-and-desist rave.

# Bash strict mode: bail on errors, undefined vars, or broken pipes.
# Saves us from shipping a release that silently failed halfway through.
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <version>" >&2
  exit 1
fi

# Version tag we want burned into the artifact's filename.
VERSION="$1"
# Where the goodies land; keeping it local so nothing leaks elsewhere.
OUTPUT_DIR="dist"
# Resolve the repo root no matter where this script got invoked from.
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Make sure the output directory exists before we start tossing files into it.
mkdir -p "$ROOT_DIR/$OUTPUT_DIR"


# Duck into the firmware folder where PlatformIO expects to be.
pushd "$ROOT_DIR/firmware" >/dev/null
# Kick the tires before we torch the build server.
echo "Running Unity tests..."
# If the Unity tests explode, we stop here—no cursed firmware escapes.
pio test -e teensy40_unity || {
  echo "Tests failed. Refusing to ship busted bits." >&2
  exit 1
}

# If we made it here, the rig's cool. Build the main firmware.
pio run -e teensy40_main
# Slide the freshly minted hex into the dist folder with a proper name.
cp .pio/build/teensy40_main/firmware.hex "$ROOT_DIR/$OUTPUT_DIR/mn42_${VERSION}.hex"
# Jump back to wherever we came from to finish the ritual.
popd >/dev/null

# Hoard the legal paperwork.
# Third-party license markdown rides shotgun...
cp "$ROOT_DIR/THIRD_PARTY_LICENSES.md" "$ROOT_DIR/$OUTPUT_DIR/"
# ...and we drag along the source license stash for good measure.
cp -r "$ROOT_DIR/firmware/LICENSES" "$ROOT_DIR/$OUTPUT_DIR/"

echo "Release artifacts ready in $OUTPUT_DIR/"
ls "$ROOT_DIR/$OUTPUT_DIR"
# Final PSA: tag your commit and let CI blast the artifacts to GitHub.
echo "Push a tag and let CI upload the goods to GitHub releases."
