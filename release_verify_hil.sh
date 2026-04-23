#!/usr/bin/env bash
# Release verification lane: host suites always run; HIL runs when available.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
UNITY_ENV="${UNITY_ENV:-teensy40_unity}"
REQUIRE_HIL="${REQUIRE_HIL:-0}"
VERIFICATION_FILE="${RELEASE_VERIFICATION_FILE:-$ROOT_DIR/.release_verification.json}"
LOG_DIR="$ROOT_DIR/logs"

mkdir -p "$LOG_DIR"

detect_port() {
  if [ -n "${TEST_PORT:-}" ]; then
    printf '%s\n' "$TEST_PORT"
    return
  fi

  shopt -s nullglob
  local ports=(/dev/ttyACM* /dev/ttyUSB* /dev/cu.usbmodem* /dev/cu.usbserial*)
  shopt -u nullglob
  if [ ${#ports[@]} -gt 0 ]; then
    printf '%s\n' "${ports[0]}"
  fi
}

run_logged() {
  local log_file="$1"
  shift

  if "$@" 2>&1 | tee "$log_file"; then
    return 0
  fi
  return 1
}

write_verification() {
  local result="$1"
  local mode="$2"
  local note="$3"
  local hil_status="$4"
  local app_status="$5"
  local bridge_status="$6"
  local system_status="$7"
  local command="$8"
  local port="$9"

  RESULT="$result" \
  MODE="$mode" \
  NOTE="$note" \
  UNITY_ENV="$UNITY_ENV" \
  REQUIRE_HIL="$REQUIRE_HIL" \
  TEST_PORT="$port" \
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

PORT="$(detect_port || true)"
MODE="host_only"
if [ -n "$PORT" ]; then
  MODE="hardware_available"
  echo "Using TEST_PORT=${PORT}"
fi

bridge_status="not_run"
app_status="not_run"
hil_status="skipped_no_port"
system_status="skipped_no_port"
failed=0
commands=()

if run_logged "$LOG_DIR/bridge-test.log" npm --prefix "$ROOT_DIR/bridge" test; then
  bridge_status="passed"
else
  bridge_status="failed"
  failed=1
fi
commands+=("npm --prefix bridge test")

if run_logged "$LOG_DIR/app-test.log" npm --prefix "$ROOT_DIR/App" test; then
  app_status="passed"
else
  app_status="failed"
  failed=1
fi
commands+=("npm --prefix App test")

if [ -n "$PORT" ]; then
  if run_logged "$LOG_DIR/unity-test.log" \
    pio -d "$ROOT_DIR/firmware" test -e "$UNITY_ENV" --without-uploading --test-port "$PORT" -vvv; then
    hil_status="passed"
  else
    hil_status="failed"
    failed=1
  fi
  commands+=("pio -d firmware test -e $UNITY_ENV --without-uploading --test-port $PORT -vvv")

  if run_logged "$LOG_DIR/system-test.log" \
    node "$ROOT_DIR/firmware/system_test/mn42_fullstack_runner.js" \
      --serial "$PORT" \
      --report "$LOG_DIR/system-test.json"; then
    system_status="passed"
  else
    system_status="failed"
    failed=1
  fi
  commands+=("node firmware/system_test/mn42_fullstack_runner.js --serial $PORT --report logs/system-test.json")
elif [ "$REQUIRE_HIL" = "1" ]; then
  hil_status="failed_no_port"
  system_status="failed_no_port"
  failed=1
fi

command_summary="$(IFS='; '; printf '%s' "${commands[*]}")"

if [ "$failed" -eq 0 ]; then
  if [ -n "$PORT" ]; then
    write_verification \
      "passed_with_hil" \
      "$MODE" \
      "Bridge, app, Unity HIL, and full-stack system lanes passed." \
      "$hil_status" \
      "$app_status" \
      "$bridge_status" \
      "$system_status" \
      "$command_summary" \
      "$PORT"
  else
    write_verification \
      "passed_host_only" \
      "$MODE" \
      "Bridge and app lanes passed; HIL and full-stack system lanes skipped because no hardware port was available." \
      "$hil_status" \
      "$app_status" \
      "$bridge_status" \
      "$system_status" \
      "$command_summary" \
      ""
  fi
  exit 0
fi

if [ -z "$PORT" ] && [ "$REQUIRE_HIL" = "1" ]; then
  note="REQUIRE_HIL=1 but no TEST_PORT was set or auto-detected."
else
  note="One or more release verification lanes failed."
fi

write_verification \
  "failed" \
  "$MODE" \
  "$note" \
  "$hil_status" \
  "$app_status" \
  "$bridge_status" \
  "$system_status" \
  "$command_summary" \
  "$PORT"

echo "$note" >&2
exit 1
