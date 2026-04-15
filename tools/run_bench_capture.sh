#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

usage() {
  cat <<'EOF'
Usage:
  tools/run_bench_capture.sh <preset> [options]

Presets:
  white25
  white50
  white100
  sweep
  wash
  blast
  overnight-soak

Options:
  --port <device>       Serial port path. Default: auto
  --baud <rate>         Serial baud rate. Default: 115200
  --duration <seconds>  Override preset duration
  --label <name>        Override output label
  --skip-upload         Do not run PlatformIO upload first
  --no-echo             Pass through to capture tool
  --help                Show this help
EOF
}

if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

if [[ "$1" == "--help" || "$1" == "-h" ]]; then
  usage
  exit 0
fi

PRESET="$1"
shift

PORT="auto"
BAUD="115200"
DURATION_OVERRIDE=""
LABEL_OVERRIDE=""
SKIP_UPLOAD="0"
NO_ECHO="0"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port)
      PORT="$2"
      shift 2
      ;;
    --baud)
      BAUD="$2"
      shift 2
      ;;
    --duration)
      DURATION_OVERRIDE="$2"
      shift 2
      ;;
    --label)
      LABEL_OVERRIDE="$2"
      shift 2
      ;;
    --skip-upload)
      SKIP_UPLOAD="1"
      shift
      ;;
    --no-echo)
      NO_ECHO="1"
      shift
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

ENV_NAME="teensy40_power_burnin"
PHASE_COMMAND=""
DURATION=""
LABEL=""

case "$PRESET" in
  white25)
    PHASE_COMMAND="phase white25"
    DURATION="300"
    LABEL="bench-white25"
    ;;
  white50)
    PHASE_COMMAND="phase white50"
    DURATION="300"
    LABEL="bench-white50"
    ;;
  white100)
    PHASE_COMMAND="phase white100"
    DURATION="300"
    LABEL="bench-white100"
    ;;
  sweep)
    PHASE_COMMAND="phase sweep"
    DURATION="300"
    LABEL="bench-sweep"
    ;;
  wash)
    PHASE_COMMAND="phase wash"
    DURATION="300"
    LABEL="bench-wash"
    ;;
  blast)
    PHASE_COMMAND="phase blast"
    DURATION="300"
    LABEL="bench-blast"
    ;;
  overnight-soak)
    PHASE_COMMAND="phase blast"
    DURATION="28800"
    LABEL="bench-overnight-soak"
    ;;
  *)
    echo "Unknown preset: $PRESET" >&2
    usage
    exit 1
    ;;
esac

if [[ -n "$DURATION_OVERRIDE" ]]; then
  DURATION="$DURATION_OVERRIDE"
fi

if [[ -n "$LABEL_OVERRIDE" ]]; then
  LABEL="$LABEL_OVERRIDE"
fi

STAMP="$(date +%Y%m%d-%H%M%S)"
CSV_PATH="logs/${LABEL}-${STAMP}.csv"
RAW_PATH="logs/${LABEL}-${STAMP}.log"
SUMMARY_PATH="logs/${LABEL}-${STAMP}-summary.md"

echo "Preset:        $PRESET"
echo "Environment:   $ENV_NAME"
echo "Phase command: $PHASE_COMMAND"
echo "Duration (s):  $DURATION"
echo "Port:          $PORT"
echo "CSV:           $CSV_PATH"
echo "Raw log:       $RAW_PATH"
echo "Summary:       $SUMMARY_PATH"

if [[ "$SKIP_UPLOAD" != "1" ]]; then
  echo
  echo "Uploading $ENV_NAME ..."
  pio -d firmware run -e "$ENV_NAME" -t upload
fi

CAPTURE_ARGS=(
  python3
  tools/capture_bench_log.py
  --port "$PORT"
  --baud "$BAUD"
  --duration "$DURATION"
  --label "$LABEL"
  --csv "$CSV_PATH"
  --raw "$RAW_PATH"
  --command "$PHASE_COMMAND"
  --command "status"
)

if [[ "$NO_ECHO" == "1" ]]; then
  CAPTURE_ARGS+=(--no-echo)
fi

echo
echo "Starting capture ..."
"${CAPTURE_ARGS[@]}"

echo
echo "Summarizing capture ..."
python3 tools/summarize_bench_log.py "$CSV_PATH" --format markdown --output "$SUMMARY_PATH"

echo
echo "Bench run complete."
echo "  CSV:     $CSV_PATH"
echo "  Raw log: $RAW_PATH"
echo "  Summary: $SUMMARY_PATH"
