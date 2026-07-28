#!/usr/bin/env python3
"""Require source changes named in docs/contracts.yml to update contract docs."""

from __future__ import annotations

import argparse
import fnmatch
import pathlib
import subprocess
import sys


def parse_contracts(path: pathlib.Path) -> dict[str, dict[str, list[str]]]:
    contracts: dict[str, dict[str, list[str]]] = {}
    current_contract: str | None = None
    current_section: str | None = None
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        stripped = raw_line.strip()
        indent = len(raw_line) - len(raw_line.lstrip())
        if not stripped or stripped.startswith("#") or stripped == "contracts:":
            continue
        if indent == 2 and stripped.endswith(":"):
            current_contract = stripped[:-1]
            contracts[current_contract] = {"source": [], "docs": []}
            current_section = None
        elif indent == 4 and stripped in {"source:", "docs:"}:
            current_section = stripped[:-1]
        elif indent == 6 and stripped.startswith("- ") and current_contract and current_section:
            contracts[current_contract][current_section].append(stripped[2:].strip())
    return contracts


def changed_files(root: pathlib.Path, base: str | None) -> list[str]:
    command = ["git", "diff", "--name-only"]
    if base:
        command.extend([base, "HEAD"])
    else:
        command.append("HEAD")
    result = subprocess.run(command, cwd=root, text=True, capture_output=True)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "git diff failed")
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def matches(path: str, patterns: list[str]) -> bool:
    return any(fnmatch.fnmatch(path, pattern) for pattern in patterns)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=".", help="Repository root")
    parser.add_argument("--base", help="Git revision to compare with HEAD")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    ledger_path = root / "docs" / "contracts.yml"
    contracts = parse_contracts(ledger_path)
    errors: list[str] = []

    for name, mapping in contracts.items():
        if not mapping["source"] or not mapping["docs"]:
            errors.append(f"{name}: source and docs entries are both required")
        for entry in mapping["docs"]:
            if not (root / entry).is_file():
                errors.append(f"{name}: missing contract document {entry}")

    changed = changed_files(root, args.base)
    for name, mapping in contracts.items():
        source_changes = [path for path in changed if matches(path, mapping["source"])]
        if source_changes and not any(matches(path, mapping["docs"]) for path in changed):
            errors.append(
                f"{name}: source changed without a mapped contract doc "
                f"({', '.join(source_changes[:3])})"
            )

    if errors:
        print("contract ledger check failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        raise SystemExit(1)
    print(f"contract ledger check passed ({len(contracts)} contracts, {len(changed)} changed files)")


if __name__ == "__main__":
    main()
