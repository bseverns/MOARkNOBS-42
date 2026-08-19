#!/usr/bin/env bash
# Local and CI test orchestrator. Pokes firmware tests if a board is around
# and always runs the bridge's JS tests so nothing sneaks by.
set -euo pipefail  # nuke on first failure, unset var, or pipe mischief
mkdir -p logs  # stash outputs where CI can snarf them

# Hardware-free firmware gates always run. These are the same portable seams
# and production build exercised by push/PR CI, so ./test.sh remains a real
# preflight even when no Teensy is connected.
GESTURE_TEST_BIN="$(mktemp -t mn42-button-gestures.XXXXXX)"
trap 'rm -f "$GESTURE_TEST_BIN"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I firmware/include \
  tools/button_gesture_timing_test.cpp -o "$GESTURE_TEST_BIN"
"$GESTURE_TEST_BIN" | tee logs/button-gesture-test.log
pio test -d firmware -e native_biquad -vvv | tee logs/native-biquad-test.log
pio test -d firmware -e native_transport -vvv | tee logs/native-transport-test.log
pio test -d firmware -e native_modulation -vvv | tee logs/native-modulation-test.log
pio test -d firmware -e native_persistence -vvv | tee logs/native-persistence-test.log
pio run -d firmware -e teensy40_main | tee logs/firmware-main-build.log

REQUIRE_HIL="${REQUIRE_HIL:-0}"
for arg in "$@"; do
  case "$arg" in
    --require-hil) REQUIRE_HIL=1 ;;
    --no-require-hil) REQUIRE_HIL=0 ;;
  esac
done

PORT="${TEST_PORT:-}"
if [ -z "$PORT" ]; then
  # No explicit port? Go spelunking for a likely Teensy serial device.
  shopt -s nullglob
  ports=(/dev/ttyACM* /dev/ttyUSB* /dev/cu.usbmodem* /dev/cu.usbserial*)
  shopt -u nullglob
  if [ ${#ports[@]} -gt 0 ]; then
    PORT="${ports[0]}"
    echo "Auto-detected TEST_PORT=$PORT"
  fi
fi

if [ "$REQUIRE_HIL" = "1" ] && [ -z "$PORT" ]; then
  echo "HIL required but TEST_PORT is not set and no serial port was auto-detected" | tee logs/unity-test.log
  : > logs/unity-test.xml
  echo "HIL required but unavailable; refusing to continue." >&2
  exit 1
fi

if [ -n "$PORT" ]; then
  # Clean the build and run Unity tests without re-flashing the board.
  pio run -d firmware -t clean
  pio test -d firmware -e teensy40_unity --without-uploading --test-port "$PORT" -vvv --junit-output logs/unity-test.xml | tee logs/unity-test.log
  # Fail the run if PlatformIO tries to sneak in its own Unity transport.
  ! grep -F ".pio/build/teensy40_unity/unity_config/unity_config.cpp" logs/unity-test.log || { echo "Autogen Unity transport detected" >&2; exit 1; }
else
  # No board? Fine—drop a placeholder so CI artifacts stay predictable.
  echo "Skipping Unity tests: TEST_PORT not set and no port auto-detected" | tee logs/unity-test.log
  : > logs/unity-test.xml
fi

# JavaScript side of the house always gets tested.
npm --prefix bridge test | tee logs/bridge-test.log
npm --prefix App test | tee logs/app-test.log

# Docs are code here; dead breadcrumbs make learner brains sad.
python3 tools/check_docs_links.py | tee logs/docs-link-check.log

if [ -n "$PORT" ]; then
  # Full-stack OSC↔firmware shakedown. Writes JSON + text logs for CI artifacts.
  node firmware/system_test/mn42_fullstack_runner.js \
    --serial "$PORT" \
    --report logs/system-test.json | tee logs/system-test.log
else
  echo "Skipping system bridge tests: TEST_PORT not set and no port auto-detected" | tee logs/system-test.log
  cat <<'EOF' > logs/system-test.json
{
  "skipped": true,
  "reason": "no serial port detected"
}
EOF
fi
