#!/usr/bin/env bash
set -euo pipefail
mkdir -p logs
pio run -d firmware -t clean
if [ -n "${TEST_PORT:-}" ]; then
  pio test -d firmware -e teensy40_unity --without-uploading --test-port "$TEST_PORT" -vvv --junit-output-path logs/unity-junit.xml | tee logs/unity.log
  if grep -F ".pio/build/teensy40_unity/unity_config/unity_config.cpp" logs/unity.log; then
    echo "Autogen Unity transport detected" >&2
    exit 1
  fi
else
  echo "Skipping Unity tests: TEST_PORT not set"
  touch logs/unity.log logs/unity-junit.xml
fi
