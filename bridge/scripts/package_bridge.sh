#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENTRY="${ROOT_DIR}/mn42_bridge.js"
DIST_DIR="${ROOT_DIR}/dist"
OUTPUT_NAME="${OUTPUT_NAME:-mn42-bridge}"
TARGETS="${TARGETS:-node22-macos-x64,node22-macos-arm64,node22-linux-x64,node22-win-x64}"
LOCAL_PKG_BIN="${ROOT_DIR}/node_modules/.bin/pkg"
PKG_BIN="${PKG_BIN:-}"
PKG_CACHE_PATH="${PKG_CACHE_PATH:-${ROOT_DIR}/.pkg-cache}"

if [[ -z "${PKG_BIN}" && -x "${LOCAL_PKG_BIN}" ]]; then
  PKG_BIN="${LOCAL_PKG_BIN}"
fi

if [[ -z "${PKG_BIN}" ]]; then
  PKG_BIN="$(command -v pkg || true)"
fi

if [[ ! -f "${ENTRY}" ]]; then
  echo "Bridge entrypoint not found: ${ENTRY}" >&2
  exit 1
fi

if [[ -z "${PKG_BIN}" ]]; then
  cat >&2 <<'EOF'
pkg is not installed or not on PATH.
Run `npm --prefix bridge ci` to install the pinned local toolchain,
or install pkg globally if you know what you're doing:
  npm install -g pkg
EOF
  exit 1
fi

mkdir -p "${DIST_DIR}"
mkdir -p "${PKG_CACHE_PATH}"
export PKG_CACHE_PATH

IFS=',' read -r -a TARGET_ARRAY <<< "${TARGETS}"
packaged_artifacts=()
for target in "${TARGET_ARRAY[@]}"; do
  target_trimmed="$(echo "${target}" | xargs)"
  if [[ -z "${target_trimmed}" ]]; then
    continue
  fi
  out_path="${DIST_DIR}/${OUTPUT_NAME}-${target_trimmed}"
  echo "Packaging ${target_trimmed} -> ${out_path}"
  "${PKG_BIN}" "${ENTRY}" --targets "${target_trimmed}" --output "${out_path}"

  shopt -s nullglob
  target_artifacts=( "${out_path}" "${out_path}.exe" "${out_path}"* )
  shopt -u nullglob

  found_any=0
  for artifact in "${target_artifacts[@]}"; do
    if [[ -f "${artifact}" ]]; then
      packaged_artifacts+=( "${artifact}" )
      found_any=1
    fi
  done

  if [[ "${found_any}" -eq 0 ]]; then
    echo "No packaged artifact produced for ${target_trimmed}" >&2
    exit 1
  fi
done

echo "Packaging complete. Artifacts in ${DIST_DIR}"
printf '%s\n' "${packaged_artifacts[@]}" | sort -u
