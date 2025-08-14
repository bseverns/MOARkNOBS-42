#!/usr/bin/env bash
set -euo pipefail
mkdir -p logs
if [ -n "${TEST_PORT:-}" ]; then
  pio run -d firmware -t clean
  pio test -d firmware -e teensy40_unity --without-uploading --test-port "$TEST_PORT" -vvv --junit-output logs/unity-test.xml | tee logs/unity-test.log
  ! grep -F ".pio/build/teensy40_unity/unity_config/unity_config.cpp" logs/unity-test.log || { echo "Autogen Unity transport detected" >&2; exit 1; }
else
  echo "Skipping Unity tests: TEST_PORT not set" | tee logs/unity-test.log
  : > logs/unity-test.xml
fi
