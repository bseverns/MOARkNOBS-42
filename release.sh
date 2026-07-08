#!/usr/bin/env bash
# Release orchestrator: optional HIL verification + deterministic hardware-test/prerelease artifact build.
set -euo pipefail

usage() {
  echo "Usage: $0 <version>" >&2
}

if [ "$#" -ne 1 ]; then
  usage
  exit 1
fi

case "$1" in
  --help|-h)
    usage
    exit 0
    ;;
  -*)
    usage
    echo "error: version must not start with '-'" >&2
    exit 1
    ;;
esac

if [ -z "$1" ]; then
  usage
  echo "error: version must not be empty" >&2
  exit 1
fi

VERSION="$1"
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

"$ROOT_DIR/release_verify_hil.sh"
"$ROOT_DIR/release_build.sh" "$VERSION"

echo "Hardware-test/prerelease artifacts ready in dist/"
echo "Push a tag and let CI upload labeled hardware-test assets to an existing GitHub release."
