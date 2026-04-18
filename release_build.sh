#!/usr/bin/env bash
# Deterministic build + packaging lane for release artifacts.
set -euo pipefail
IFS=$'\n\t'
umask 022

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <version>" >&2
  exit 1
fi

VERSION="$1"
OUTPUT_DIR="dist"
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_ENV="teensy40_main"
FIRMWARE_NAME="mn42_${VERSION}.hex"
FABRICATION_NAME="fabrication.zip"
SOURCE_EXPORT_NAME="mn42_${VERSION}_source.tar.gz"
MANIFEST_NAME="manifest.json"
CHECKSUMS_NAME="SHA256SUMS.txt"
VERIFICATION_NAME="release_verification.json"
PROJECT_DIR="$ROOT_DIR/firmware"
FABRICATION_DIR="$ROOT_DIR/hardware/fabrication"
VERIFICATION_SOURCE_FILE="${RELEASE_VERIFICATION_FILE:-$ROOT_DIR/.release_verification.json}"
VERIFICATION_DIST_FILE="$ROOT_DIR/$OUTPUT_DIR/$VERIFICATION_NAME"

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

pushd "$PROJECT_DIR" >/dev/null

CLEAN_CMD=(pio run -t clean -e "$BUILD_ENV")
echo "Scrubbing previous build outputs..."
"${CLEAN_CMD[@]}"

BUILD_CMD=(pio run -e "$BUILD_ENV")
echo "Building firmware..."
"${BUILD_CMD[@]}"

cp ".pio/build/${BUILD_ENV}/firmware.hex" "$ROOT_DIR/$OUTPUT_DIR/$FIRMWARE_NAME"
popd >/dev/null

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
        info.date_time = (1980, 1, 1, 0, 0, 0)
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o100644 << 16
        with path.open("rb") as fh:
            zf.writestr(info, fh.read())
PY

python3 "$ROOT_DIR/tools/export_release_source.py" \
  --root "$ROOT_DIR" \
  --version "$VERSION" \
  --output "$ROOT_DIR/$OUTPUT_DIR/$SOURCE_EXPORT_NAME"

if [ -f "$VERIFICATION_SOURCE_FILE" ]; then
  cp "$VERIFICATION_SOURCE_FILE" "$VERIFICATION_DIST_FILE"
else
  python3 - "$VERIFICATION_DIST_FILE" <<'PY'
import datetime as _dt
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1]).resolve()
path.parent.mkdir(parents=True, exist_ok=True)
payload = {
    "generated_at_utc": _dt.datetime.now(tz=_dt.timezone.utc).isoformat(),
    "lane": "release_verify_hil",
    "result": "not_provided",
    "note": "No release verification summary file was produced before artifact packaging.",
    "tests": {
        "hil_unity": {"status": "unknown"},
        "app_suite": {"status": "unknown"},
        "bridge_suite": {"status": "unknown"},
        "system_fullstack": {"status": "unknown"},
    },
}
path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
PY
fi

cp "$ROOT_DIR/THIRD_PARTY_LICENSES.md" "$ROOT_DIR/$OUTPUT_DIR/"
rm -rf "$ROOT_DIR/$OUTPUT_DIR/LICENSES"
cp -r "$ROOT_DIR/firmware/LICENSES" "$ROOT_DIR/$OUTPUT_DIR/"

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
  --artifact "source_export=$ROOT_DIR/$OUTPUT_DIR/$SOURCE_EXPORT_NAME" \
  --artifact "verification=$VERIFICATION_DIST_FILE" \
  --artifact "third_party_licenses=$ROOT_DIR/$OUTPUT_DIR/THIRD_PARTY_LICENSES.md" \
  --verification-file "$VERIFICATION_DIST_FILE" \
  --pio-home "$PIO_HOME" \
  --step "clean=$CLEAN_CMD_STR" \
  --step "build=$BUILD_CMD_STR"

python3 "$ROOT_DIR/tools/write_checksums.py" \
  --output "$ROOT_DIR/$OUTPUT_DIR/$CHECKSUMS_NAME" \
  --file "$ROOT_DIR/$OUTPUT_DIR/$FIRMWARE_NAME" \
  --file "$ROOT_DIR/$OUTPUT_DIR/$FABRICATION_NAME" \
  --file "$ROOT_DIR/$OUTPUT_DIR/$SOURCE_EXPORT_NAME" \
  --file "$VERIFICATION_DIST_FILE" \
  --file "$ROOT_DIR/$OUTPUT_DIR/$MANIFEST_NAME" \
  --file "$ROOT_DIR/$OUTPUT_DIR/THIRD_PARTY_LICENSES.md"

echo "Build artifacts ready in $OUTPUT_DIR/"
ls "$ROOT_DIR/$OUTPUT_DIR"
