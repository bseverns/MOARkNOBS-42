#!/usr/bin/env python3
"""Check first-party source comments against the house comment style."""

from __future__ import annotations

import argparse
import pathlib
import re


SOURCE_ROOTS = ("firmware", "App", "bridge", "tools", "scripts")
SOURCE_SUFFIXES = {".h", ".hpp", ".c", ".cpp", ".js", ".mjs"}
SKIP_PARTS = {
    ".git",
    ".pio",
    ".platformio",
    ".venv",
    ".venv-docs",
    "__pycache__",
    "node_modules",
    "site",
    "dist",
}
DOXYGEN_TAG_RE = re.compile(r"@(brief|param|return|returns|details|ref)\b")
INLINE_BLOCK_RE = re.compile(r"/\*([^*].*?)\*/")
NO_OP_RE = re.compile(r"/\*\s*(no-op|noop)\s*\*/", re.IGNORECASE)
STAR_LADDER_RE = re.compile(r"^\s+\* ?")


def should_skip(path: pathlib.Path, root: pathlib.Path) -> bool:
    rel = path.relative_to(root)
    parts = set(rel.parts)
    if parts & SKIP_PARTS:
        return True
    return len(rel.parts) >= 2 and rel.parts[0] == "firmware" and rel.parts[1] == "lib"


def iter_source_files(root: pathlib.Path) -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for source_root in SOURCE_ROOTS:
        base = root / source_root
        if not base.exists():
            continue
        for path in sorted(base.rglob("*")):
            if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
                continue
            if should_skip(path, root):
                continue
            files.append(path)
    return files


def inline_block_allowed(line: str, match: re.Match[str]) -> bool:
    content = match.group(1).strip()
    before = line[: match.start()]
    if content.startswith("eslint"):
        return True
    if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", content) and not any(
        token in before for token in ("{", ";", "=")
    ):
        return True
    return False


def check_file(root: pathlib.Path, path: pathlib.Path) -> list[str]:
    rel = path.relative_to(root)
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except UnicodeDecodeError:
        return [f"{rel}: non-UTF-8 source skipped by style check"]

    errors: list[str] = []
    in_block = False

    for line_no, line in enumerate(lines, start=1):
        stripped = line.strip()

        if "/**" in line:
            errors.append(f"{rel}:{line_no}: use plain /* blocks or //, not /** Doxygen blocks")

        if NO_OP_RE.search(line):
            errors.append(f"{rel}:{line_no}: use // no-op instead of inline /* no-op */")

        if DOXYGEN_TAG_RE.search(line):
            errors.append(f"{rel}:{line_no}: remove Doxygen tag syntax from comments")

        for match in INLINE_BLOCK_RE.finditer(line):
            if not inline_block_allowed(line, match):
                errors.append(
                    f"{rel}:{line_no}: avoid inline block comments except unused parameter names"
                )

        if in_block:
            if stripped == "*/":
                in_block = False
                continue
            if STAR_LADDER_RE.match(line):
                errors.append(f"{rel}:{line_no}: remove leading * ladder from block comment")
            continue

        if stripped == "/*":
            in_block = True

    return errors


def main() -> None:
    parser = argparse.ArgumentParser(description="Check house comment style for source files.")
    parser.add_argument("--root", default=".", help="Repository root")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    errors: list[str] = []
    for path in iter_source_files(root):
        errors.extend(check_file(root, path))

    if errors:
        print("Comment style check failed:")
        for error in errors:
            print(f"  - {error}")
        raise SystemExit(1)

    print("comment style check passed")


if __name__ == "__main__":
    main()
