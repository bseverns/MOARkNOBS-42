#!/usr/bin/env bash
# Local test orchestrator for firmware + bridge.
set -euo pipefail  # strict mode so any failure kills the vibe early
mkdir -p logs      # stash logs here so CI and humans can dig through later

# Figure out where the Teensy is plugged in.
PORT="${TEST_PORT:-}"
if [ -z "$PORT" ]; then
  shopt -s nullglob  # stop globbing from screaming if no matches
  ports=(/dev/ttyACM* /dev/ttyUSB* /dev/cu.usbmodem* /dev/cu.usbserial*)
  shopt -u nullglob
  if [ ${#ports[@]} -gt 0 ]; then
    PORT="${ports[0]}"
    echo "Auto-detected TEST_PORT=$PORT"
  fi
fi

if [ -n "$PORT" ]; then
  # Clean build products to make sure nothing stale sneaks in.
  pio run -d firmware -t clean
  # Run Unity tests without flashing new firmware; stream results to logs.
  pio test -d firmware -e teensy40_unity --without-uploading --test-port "$PORT" -vvv --junit-output logs/unity-test.xml | tee logs/unity-test.log
  # Guard against PlatformIO auto-generating a transport we explicitly avoid.
  ! grep -F ".pio/build/teensy40_unity/unity_config/unity_config.cpp" logs/unity-test.log || { echo "Autogen Unity transport detected" >&2; exit 1; }
else
  # Bail gracefully if no hardware is present.
  echo "Skipping Unity tests: TEST_PORT not set and no port auto-detected" | tee logs/unity-test.log
  : > logs/unity-test.xml
fi

# Run the bridge's JS tests for good measure.
npm --prefix bridge test | tee logs/bridge-test.log
