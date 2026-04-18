#!/usr/bin/env bash
# Release orchestrator: optional HIL verification + deterministic artifact build.
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <version>" >&2
  exit 1
fi

VERSION="$1"
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

"$ROOT_DIR/release_verify_hil.sh"
"$ROOT_DIR/release_build.sh" "$VERSION"

echo "Release artifacts ready in dist/"
echo "Push a tag and let CI upload the goods to GitHub releases."
