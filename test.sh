#!/usr/bin/env bash
set -euo pipefail
mkdir -p logs

PORT="${TEST_PORT:-}"
if [ -z "$PORT" ]; then
  shopt -s nullglob
  ports=(/dev/ttyACM* /dev/ttyUSB* /dev/cu.usbmodem* /dev/cu.usbserial*)
  shopt -u nullglob
  if [ ${#ports[@]} -gt 0 ]; then
    PORT="${ports[0]}"
    echo "Auto-detected TEST_PORT=$PORT"
  fi
fi

if [ -n "$PORT" ]; then
  pio run -d firmware -t clean
  pio test -d firmware -e teensy40_unity --without-uploading --test-port "$PORT" -vvv --junit-output logs/unity-test.xml | tee logs/unity-test.log
  ! grep -F ".pio/build/teensy40_unity/unity_config/unity_config.cpp" logs/unity-test.log || { echo "Autogen Unity transport detected" >&2; exit 1; }
else
  echo "Skipping Unity tests: TEST_PORT not set and no port auto-detected" | tee logs/unity-test.log
  : > logs/unity-test.xml
fi

npm --prefix bridge test | tee logs/bridge-test.log
