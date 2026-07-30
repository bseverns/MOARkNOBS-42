#!/usr/bin/env python3
"""Enforce wiki 'fast map' contract against canonical repo docs."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys


EXPECTED_CANONICAL = {
    "Home.md": "README.md",
    "Getting-Started.md": "docs/getting-started/QuickstartForPerformers.md",
    "Playable-Walkthrough.md": "docs/getting-started/MusicianFirstGuide.md",
    "Troubleshooting.md": "docs/validation/FailureFirst.md",
    "Developer-Setup.md": "docs/project/ProcessOverview.md",
    "System-Architecture.md": "README.md",
    "Firmware.md": "firmware/README.md",
    "Hardware.md": "hardware/README.md",
    "WebSerial-App.md": "App/README.md",
    "OSC-Bridge.md": "bridge/README.md",
    "Testing.md": "docs/validation/TESTING.md",
    "Release-Process.md": "docs/release/ReleaseGuide.md",
    "History-and-Roadmap.md": "docs/project/HISTORY.md",
}

REQUIRED_CURRENT_CLAIMS = {
    "System-Architecture.md": (
        "generation-backed LittleFS",
        "docs/reference/PersistenceContract.md",
    ),
    "Firmware.md": (
        "generation-backed LittleFS",
        "docs/reference/PersistenceContract.md",
    ),
}

STALE_CLAIM_PATTERNS = {
    "System-Architecture.md": (
        re.compile(r"EEPROM stores config", re.IGNORECASE),
    ),
    "Firmware.md": (
        re.compile(r"ConfigManager`\s*-\s*EEPROM persistence", re.IGNORECASE),
    ),
}

CANONICAL_LINE_RE = re.compile(r"^Canonical source:\s*(.+)$", re.MULTILINE)


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate wiki canonical-source policy.")
    parser.add_argument("--root", default=".", help="Repository root")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    wiki_dir = root / "wiki"
    errors: list[str] = []

    for wiki_name, expected_path in EXPECTED_CANONICAL.items():
        wiki_path = wiki_dir / wiki_name
        if not wiki_path.exists():
            errors.append(f"missing wiki page: {wiki_name}")
            continue

        text = wiki_path.read_text(encoding="utf-8")
        match = CANONICAL_LINE_RE.search(text)
        if not match:
            errors.append(f"{wiki_name}: missing 'Canonical source:' line")
            continue

        declared = match.group(1).strip().strip("`")
        if declared != expected_path:
            errors.append(f"{wiki_name}: canonical source '{declared}' != expected '{expected_path}'")

        target = (root / expected_path).resolve()
        if not target.exists():
            errors.append(f"{wiki_name}: canonical source does not exist '{expected_path}'")

        for required_claim in REQUIRED_CURRENT_CLAIMS.get(wiki_name, ()):
            if required_claim not in text:
                errors.append(
                    f"{wiki_name}: missing current canonical claim '{required_claim}'"
                )

        for stale_pattern in STALE_CLAIM_PATTERNS.get(wiki_name, ()):
            if stale_pattern.search(text):
                errors.append(
                    f"{wiki_name}: contains stale claim matching '{stale_pattern.pattern}'"
                )

    if errors:
        for err in errors:
            print(err)
        print(f"\nwiki contract check failed: {len(errors)} issue(s)", file=sys.stderr)
        raise SystemExit(1)

    print(f"wiki contract check passed ({len(EXPECTED_CANONICAL)} pages)")


if __name__ == "__main__":
    main()
