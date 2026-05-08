#!/usr/bin/env python3
"""Create a dated validation report from a docs/tools template."""

from __future__ import annotations

import argparse
import datetime as dt
import pathlib
import re


TEMPLATES = {
    "soak": "five-minute-soak-report-template.md",
    "ef": "ef-stability-report-template.md",
    "ext-clock": "ext-clock-starvation-report-template.md",
    "panic": "panic-baseline-report-template.md",
}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=sorted(TEMPLATES))
    parser.add_argument("--root", default=".", help="Repository root")
    parser.add_argument("--date", default=dt.date.today().isoformat(), help="YYYY-MM-DD")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    template = root / "docs/tools" / TEMPLATES[args.kind]
    reports = root / "docs/validation/reports"
    reports.mkdir(parents=True, exist_ok=True)

    safe_kind = re.sub(r"[^a-z0-9-]+", "-", args.kind.lower()).strip("-")
    target = reports / f"{args.date}-{safe_kind}.md"
    target.write_text(template.read_text(encoding="utf-8"), encoding="utf-8")
    print(target)


if __name__ == "__main__":
    main()
