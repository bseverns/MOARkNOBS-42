#!/usr/bin/env python3
"""Verify generated consumers of the canonical MN42 contract are current."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

from generate_contract_artifacts import expected_artifacts, load_sources


DOCUMENTED_SCHEMA_PATTERNS = {
    "docs/reference/assumption-ledger.md": r"contract is locked to schema v(\d+)",
    "docs/reference/MN42LineProtocol.md": r'"schema_version":(\d+)',
    "docs/reference/SerialProtocol.md": r'"schema_version":(\d+)',
    "docs/guides/WebSerial.md": r'"schema_version":\s*(\d+)',
}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=".", help="Repository root")
    args = parser.parse_args()
    root = pathlib.Path(args.root).resolve()
    contract, _ = load_sources(root)
    schema_version = contract["manifest"]["schema_version"]
    errors: list[str] = []

    for path, expected in expected_artifacts(root).items():
        relative = path.relative_to(root)
        actual = path.read_text(encoding="utf-8") if path.exists() else None
        if actual != expected:
            errors.append(
                f"{relative}: stale generated artifact; run "
                "python3 tools/generate_contract_artifacts.py --root ."
            )

    for relative, pattern in DOCUMENTED_SCHEMA_PATTERNS.items():
        text = (root / relative).read_text(encoding="utf-8")
        match = re.search(pattern, text)
        if not match:
            errors.append(f"{relative}: documented schema version is missing")
            continue
        documented = int(match.group(1))
        if documented != schema_version:
            errors.append(
                f"{relative}: documented schema {documented} != canonical {schema_version}"
            )

    if errors:
        for error in errors:
            print(error)
        print(f"\ncontract sync check failed: {len(errors)} mismatch(es)", file=sys.stderr)
        raise SystemExit(1)

    print("contract sync check passed")


if __name__ == "__main__":
    main()
