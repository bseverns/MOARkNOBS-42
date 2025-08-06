#!/usr/bin/env bash
# Release helper: builds firmware and bundles license file.
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
pio run -e teensy40_main
cp .pio/build/teensy40_main/firmware.hex "$ROOT_DIR/$OUTPUT_DIR/mn42_${VERSION}.hex"
popd >/dev/null

cp "$ROOT_DIR/THIRD_PARTY_LICENSES.md" "$ROOT_DIR/$OUTPUT_DIR/"

echo "Release artifacts ready in $OUTPUT_DIR/"
ls "$ROOT_DIR/$OUTPUT_DIR"
