#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENTRY="${ROOT_DIR}/mn42_bridge.js"
DIST_DIR="${ROOT_DIR}/dist"
OUTPUT_NAME="${OUTPUT_NAME:-mn42-bridge}"
TARGETS="${TARGETS:-node20-macos-x64,node20-linux-x64,node20-win-x64}"
PKG_BIN="${PKG_BIN:-$(command -v pkg || true)}"

if [[ ! -f "${ENTRY}" ]]; then
  echo "Bridge entrypoint not found: ${ENTRY}" >&2
  exit 1
fi

if [[ -z "${PKG_BIN}" ]]; then
  cat >&2 <<'EOF'
pkg is not installed or not on PATH.
Install one of these, then rerun:
  npm --prefix bridge install --save-dev pkg
  npm install -g pkg
EOF
  exit 1
fi

mkdir -p "${DIST_DIR}"

IFS=',' read -r -a TARGET_ARRAY <<< "${TARGETS}"
for target in "${TARGET_ARRAY[@]}"; do
  target_trimmed="$(echo "${target}" | xargs)"
  if [[ -z "${target_trimmed}" ]]; then
    continue
  fi
  out_path="${DIST_DIR}/${OUTPUT_NAME}-${target_trimmed}"
  echo "Packaging ${target_trimmed} -> ${out_path}"
  "${PKG_BIN}" "${ENTRY}" --targets "${target_trimmed}" --output "${out_path}"
done

echo "Packaging complete. Artifacts in ${DIST_DIR}"
