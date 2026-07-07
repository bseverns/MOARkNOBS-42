#!/usr/bin/env python3
"""Tiny local Markdown link gremlin hunter.

Keeps the notebook from sending learners into dead air. It only checks files
inside this repo; external links can rot on their own time.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {".git", ".pio", "node_modules", "site"}
LINK_RE = re.compile(r"(?<!!)\[[^\]]*\]\(([^)]+)\)|!\[[^\]]*\]\(([^)]+)\)")
HEADING_RE = re.compile(r"^(#{1,6})\s+(.+?)\s*#*\s*$", re.M)


def iter_markdown() -> list[Path]:
    return [p for p in ROOT.rglob("*.md") if not (SKIP_DIRS & set(p.relative_to(ROOT).parts))]


def github_slug(text: str) -> str:
    text = re.sub(r"<[^>]+>", "", text).strip().lower()
    text = re.sub(r"[`*_~]", "", text)
    text = re.sub(r"[^a-z0-9 _-]", "", text)
    return text.replace(" ", "-")


def anchors(path: Path) -> set[str]:
    seen: dict[str, int] = {}
    found: set[str] = set()
    for _, heading in HEADING_RE.findall(path.read_text(encoding="utf-8", errors="ignore")):
        slug = github_slug(heading)
        count = seen.get(slug, 0)
        seen[slug] = count + 1
        found.add(slug if count == 0 else f"{slug}-{count}")
    return found


def target_path(source: Path, raw: str) -> Path:
    target = unquote(raw.split("#", 1)[0].split("?", 1)[0].strip())
    if not target:
        return source
    base = ROOT if target.startswith("/") else source.parent
    path = (base / target.lstrip("/")).resolve()
    if path.is_dir():
        for landing in ("README.md", "index.md", "index.html"):
            if (path / landing).exists():
                return path / landing
    return path


def main() -> int:
    broken: list[str] = []
    anchor_cache: dict[Path, set[str]] = {}

    for md in iter_markdown():
        text = md.read_text(encoding="utf-8", errors="ignore")
        for match in LINK_RE.finditer(text):
            raw = (match.group(1) or match.group(2) or "").strip()
            if not raw or raw.startswith(("#", "http://", "https://", "mailto:", "data:")):
                continue
            path = target_path(md, raw)
            line = text.count("\n", 0, match.start()) + 1
            label = f"{md.relative_to(ROOT)}:{line} -> {raw}"
            if not path.exists():
                broken.append(f"{label} (missing file)")
                continue
            if "#" not in raw:
                continue
            anchor = unquote(raw.split("#", 1)[1]).strip().lower()
            if not anchor or path.suffix.lower() not in {".md", ".markdown"}:
                continue
            anchor_cache.setdefault(path, anchors(path))
            if anchor not in anchor_cache[path]:
                broken.append(f"{label} (missing #{anchor})")

    if broken:
        print("Markdown link check found busted breadcrumbs:")
        print("\n".join(f"- {item}" for item in broken))
        return 1
    print("Markdown links look wired up.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
