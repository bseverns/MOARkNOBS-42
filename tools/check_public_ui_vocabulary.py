#!/usr/bin/env python3
"""Reject retired App/Bridge labels in musician-facing front-door docs."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys


PUBLIC_DOCS = (
    "README.md",
    "docs/index.md",
    "docs/getting-started/StartHere.md",
    "docs/getting-started/FirstFiveMinutes.md",
    "docs/getting-started/QuickstartForPerformers.md",
    "docs/getting-started/GuidedRoutes.md",
    "docs/getting-started/ObjectCard.md",
    "docs/getting-started/SystemMap.md",
    "docs/getting-started/WhyMN42.md",
    "docs/getting-started/ConfigureWithoutRecompiling.md",
    "docs/guides/Configurator.md",
    "docs/guides/MidiInputMapping.md",
    "docs/guides/BridgeForPerformers.md",
    "docs/bridge/BridgeDocsMap.md",
    "docs/bridge/BridgeOperatorReference.md",
    "docs/reference/KnownGoodHostRecipes.md",
)


RULES = (
    (re.compile(r"\bBasic mode\b", re.IGNORECASE), "Configure"),
    (re.compile(r"\bAdvanced mode\b", re.IGNORECASE), "Lab or Diagnostics, depending on the product"),
    (re.compile(r"\*\*Basic\*\*"), "**Configure**"),
    (re.compile(r"\*\*Advanced\*\*"), "**Lab** or **Diagnostics**"),
    (re.compile(r"\bMappings mode\b", re.IGNORECASE), "Bridge Routing"),
    (re.compile(r"\bBridge Stage mode\b", re.IGNORECASE), "Bridge Monitor"),
    (re.compile(r"\bOne Signal Path\b"), "Who Controls This Slot?"),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=".", help="Repository root")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = pathlib.Path(args.root).resolve()
    failures: list[str] = []

    for relative in PUBLIC_DOCS:
        path = root / relative
        if not path.is_file():
            failures.append(f"{relative}: missing public documentation file")
            continue
        for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            for pattern, replacement in RULES:
                if pattern.search(line):
                    failures.append(
                        f"{relative}:{line_number}: retired public UI vocabulary "
                        f"matches {pattern.pattern!r}; use {replacement}"
                    )

    if failures:
        print("public UI vocabulary check failed:", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1

    print(f"public UI vocabulary check passed ({len(PUBLIC_DOCS)} front-door docs)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
