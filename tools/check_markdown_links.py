#!/usr/bin/env python3
"""Check local markdown links for docs and wiki files."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys
from urllib.parse import unquote


DEFAULT_TARGETS = (
    "README.md",
    "docs",
    "wiki",
    "firmware/README.md",
    "bridge/README.md",
    "App/README.md",
)

INLINE_LINK_RE = re.compile(r"(!?\[[^\]]*\])\(([^)]+)\)")
HEADING_RE = re.compile(r"^(#{1,6})\s+(.*)$", re.MULTILINE)


def strip_code_fences(text: str) -> str:
    lines = text.splitlines()
    cleaned: list[str] = []
    in_fence = False
    fence_marker = ""
    for line in lines:
        marker_match = re.match(r"^\s*(```|~~~)", line)
        if marker_match:
            marker = marker_match.group(1)
            if not in_fence:
                in_fence = True
                fence_marker = marker
            elif marker == fence_marker:
                in_fence = False
                fence_marker = ""
            continue
        if not in_fence:
            cleaned.append(line)
    return "\n".join(cleaned)


def slugify(heading: str) -> str:
    heading = heading.strip().lower()
    heading = re.sub(r"[^\w\s-]", "", heading)
    heading = re.sub(r"\s", "-", heading)
    return heading.strip("-")


def heading_anchors(markdown: str) -> set[str]:
    anchors: set[str] = set()
    counts: dict[str, int] = {}
    for match in HEADING_RE.finditer(markdown):
        base = slugify(match.group(2))
        if not base:
            continue
        suffix = counts.get(base, 0)
        anchor = base if suffix == 0 else f"{base}-{suffix}"
        counts[base] = suffix + 1
        anchors.add(anchor)
    return anchors


def is_external(target: str) -> bool:
    lowered = target.lower()
    return lowered.startswith(("http://", "https://", "mailto:", "tel:", "data:", "javascript:"))


def normalize_target(raw_target: str) -> str:
    target = raw_target.strip()
    if target.startswith("<") and target.endswith(">"):
        target = target[1:-1].strip()
    if " " in target and not target.startswith("#"):
        target = target.split(" ", 1)[0]
    return unquote(target)


def iter_markdown_files(root: pathlib.Path, targets: list[str]) -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for item in targets:
        path = (root / item).resolve()
        if path.is_file() and path.suffix.lower() == ".md":
            files.append(path)
            continue
        if path.is_dir():
            for candidate in sorted(path.rglob("*.md")):
                files.append(candidate.resolve())
    return sorted(set(files))


def validate_links(root: pathlib.Path, markdown_file: pathlib.Path) -> list[str]:
    text = markdown_file.read_text(encoding="utf-8")
    stripped = strip_code_fences(text)
    errors: list[str] = []
    source_anchors = heading_anchors(text)

    for _, raw_target in INLINE_LINK_RE.findall(stripped):
        target = normalize_target(raw_target)
        if not target or is_external(target):
            continue

        path_part, anchor = (target.split("#", 1) + [""])[:2] if "#" in target else (target, "")

        if path_part.startswith("#") or (not path_part and anchor):
            if anchor and anchor not in source_anchors:
                errors.append(f"{markdown_file}: missing local anchor '#{anchor}'")
            continue

        resolved = (markdown_file.parent / path_part).resolve()
        if not resolved.exists():
            errors.append(f"{markdown_file}: missing link target '{target}'")
            continue

        if anchor and resolved.is_file() and resolved.suffix.lower() == ".md":
            target_anchors = heading_anchors(resolved.read_text(encoding="utf-8"))
            if anchor not in target_anchors:
                errors.append(f"{markdown_file}: missing anchor '{anchor}' in '{path_part}'")

        try:
            resolved.relative_to(root)
        except ValueError:
            errors.append(f"{markdown_file}: link escapes repo root '{target}'")
    return errors


def main() -> None:
    parser = argparse.ArgumentParser(description="Check local markdown links.")
    parser.add_argument("--root", default=".", help="Repository root")
    parser.add_argument("targets", nargs="*", default=list(DEFAULT_TARGETS), help="Files/dirs to scan")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    files = iter_markdown_files(root, args.targets)
    errors: list[str] = []
    for markdown_file in files:
        errors.extend(validate_links(root, markdown_file))

    if errors:
        for err in errors:
            print(err)
        print(f"\nlink check failed: {len(errors)} issue(s)", file=sys.stderr)
        raise SystemExit(1)

    print(f"markdown link check passed ({len(files)} files)")


if __name__ == "__main__":
    main()
