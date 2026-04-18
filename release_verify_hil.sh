#!/usr/bin/env bash
# Optional hardware-in-loop verification lane for release prep.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
UNITY_ENV="${UNITY_ENV:-teensy40_unity}"
REQUIRE_HIL="${REQUIRE_HIL:-0}"
VERIFICATION_FILE="${RELEASE_VERIFICATION_FILE:-$ROOT_DIR/.release_verification.json}"

write_verification() {
  local result="$1"
  local mode="$2"
  local note="$3"
  local hil_status="$4"
  local app_status="$5"
  local bridge_status="$6"
  local system_status="$7"
  local command="$8"

  RESULT="$result" \
  MODE="$mode" \
  NOTE="$note" \
  UNITY_ENV="$UNITY_ENV" \
  REQUIRE_HIL="$REQUIRE_HIL" \
  TEST_PORT="${TEST_PORT:-}" \
  HIL_STATUS="$hil_status" \
  APP_STATUS="$app_status" \
  BRIDGE_STATUS="$bridge_status" \
  SYSTEM_STATUS="$system_status" \
  COMMAND="$command" \
  python3 - "$VERIFICATION_FILE" <<'PY'
import datetime as _dt
import json
import os
import pathlib
import sys

def _none_if_blank(value: str | None):
    if value is None:
        return None
    value = value.strip()
    return value or None

payload = {
    "generated_at_utc": _dt.datetime.now(tz=_dt.timezone.utc).isoformat(),
    "lane": "release_verify_hil",
    "require_hil": os.environ.get("REQUIRE_HIL", "0") == "1",
    "unity_env": os.environ.get("UNITY_ENV"),
    "test_port": _none_if_blank(os.environ.get("TEST_PORT")),
    "result": os.environ.get("RESULT"),
    "mode": os.environ.get("MODE"),
    "command": os.environ.get("COMMAND"),
    "note": os.environ.get("NOTE"),
    "tests": {
        "hil_unity": {"status": os.environ.get("HIL_STATUS")},
        "app_suite": {"status": os.environ.get("APP_STATUS")},
        "bridge_suite": {"status": os.environ.get("BRIDGE_STATUS")},
        "system_fullstack": {"status": os.environ.get("SYSTEM_STATUS")},
    },
}

path = pathlib.Path(sys.argv[1]).resolve()
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
PY
}

echo "Running release verification lane (UNITY_ENV=${UNITY_ENV}, REQUIRE_HIL=${REQUIRE_HIL})"
echo "Verification summary file: ${VERIFICATION_FILE}"

if [ "$REQUIRE_HIL" = "1" ]; then
  if [ -z "${TEST_PORT:-}" ]; then
    write_verification \
      "required_failed_no_port" \
      "required" \
      "REQUIRE_HIL=1 but TEST_PORT is unset." \
      "failed_no_port" \
      "not_run" \
      "not_run" \
      "not_run" \
      "./test.sh --require-hil"
    echo "HIL required but TEST_PORT is unset." >&2
    exit 1
  fi

  if "$ROOT_DIR/test.sh" --require-hil; then
    write_verification \
      "required_passed" \
      "required" \
      "Required HIL lane passed through test.sh." \
      "passed" \
      "passed" \
      "passed" \
      "passed" \
      "./test.sh --require-hil"
  else
    write_verification \
      "required_failed" \
      "required" \
      "Required HIL lane failed in test.sh." \
      "failed" \
      "failed_or_unknown" \
      "failed_or_unknown" \
      "failed_or_unknown" \
      "./test.sh --require-hil"
    exit 1
  fi
  exit 0
fi

if [ -n "${TEST_PORT:-}" ]; then
  if pio -d "$ROOT_DIR/firmware" test -e "$UNITY_ENV" --without-uploading --test-port "$TEST_PORT" -vvv; then
    write_verification \
      "optional_unity_passed" \
      "optional" \
      "Optional lane executed Unity-only verification." \
      "passed_optional" \
      "not_run" \
      "not_run" \
      "not_run" \
      "pio -d \"$ROOT_DIR/firmware\" test -e \"$UNITY_ENV\" --without-uploading --test-port \"$TEST_PORT\" -vvv"
  else
    write_verification \
      "optional_unity_failed" \
      "optional" \
      "Optional Unity verification failed." \
      "failed_optional" \
      "not_run" \
      "not_run" \
      "not_run" \
      "pio -d \"$ROOT_DIR/firmware\" test -e \"$UNITY_ENV\" --without-uploading --test-port \"$TEST_PORT\" -vvv"
    exit 1
  fi
else
  write_verification \
    "optional_skipped_no_port" \
    "optional" \
    "TEST_PORT was unset; optional HIL checks were skipped." \
    "skipped_no_port" \
    "not_run" \
    "not_run" \
    "not_run" \
    "none"
  echo "Skipping required hardware checks: TEST_PORT not set (set REQUIRE_HIL=1 to enforce)."
fi
