#!/usr/bin/env bash
# Render the MOARkNOBS-42 narrative report to PDF via Pandoc.
# Supports either Tectonic (default) or any PDF engine Pandoc knows about.
# Usage: ./tools/render_report.sh [output-dir]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INPUT_FILE="$ROOT_DIR/docs/report-CDSsample.md"
OUTPUT_DIR="${1:-$ROOT_DIR/docs/dist}"
OUTPUT_FILE="$OUTPUT_DIR/MOARkNOBS-42-report.pdf"
PDF_ENGINE="${PDF_ENGINE:-tectonic}"

if [[ ! -f "$INPUT_FILE" ]]; then
  echo "Report markdown not found at $INPUT_FILE" >&2
  exit 1
fi

if ! command -v pandoc >/dev/null 2>&1; then
  echo "pandoc is required. Install it from https://pandoc.org/install/" >&2
  exit 1
fi

if [[ "$PDF_ENGINE" == "tectonic" ]] && ! command -v tectonic >/dev/null 2>&1; then
  echo "Tectonic PDF engine not found. Install it (https://tectonic-typesetting.github.io/)" >&2
  echo "or re-run with PDF_ENGINE=xelatex ./tools/render_report.sh" >&2
  exit 1
fi

mkdir -p "$OUTPUT_DIR"

pandoc "$INPUT_FILE" \
  --from=markdown+yaml_metadata_block+link_attributes+implicit_figures+smart \
  --pdf-engine="$PDF_ENGINE" \
  --toc \
  --number-sections \
  --resource-path="$ROOT_DIR/docs:$ROOT_DIR" \
  --output="$OUTPUT_FILE"

echo "PDF written to $OUTPUT_FILE"
