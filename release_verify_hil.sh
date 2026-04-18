#!/usr/bin/env bash
# Optional hardware-in-loop verification lane for release prep.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
UNITY_ENV="${UNITY_ENV:-teensy40_unity}"
REQUIRE_HIL="${REQUIRE_HIL:-0}"

echo "Running release verification lane (UNITY_ENV=${UNITY_ENV}, REQUIRE_HIL=${REQUIRE_HIL})"

if [ "$REQUIRE_HIL" = "1" ]; then
  "$ROOT_DIR/test.sh" --require-hil
  exit 0
fi

if [ -n "${TEST_PORT:-}" ]; then
  pio -d "$ROOT_DIR/firmware" test -e "$UNITY_ENV" --without-uploading --test-port "$TEST_PORT" -vvv
else
  echo "Skipping required hardware checks: TEST_PORT not set (set REQUIRE_HIL=1 to enforce)."
fi
