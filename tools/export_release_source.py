#!/usr/bin/env python3
"""Create a deterministic source export for release uploads."""

from __future__ import annotations

import argparse
import fnmatch
import pathlib
import subprocess
import zipfile


EXCLUDE_GLOBS = (
    ".git/**",
    ".venv/**",
    ".pio/**",
    ".pio-home/**",
    ".pio-cache/**",
    ".platformio/**",
    "logs/**",
    "App/test-results/**",
    "**/.DS_Store",
    "**/__pycache__/**",
    "**/.pytest_cache/**",
    "dist/**",
    "**/tmp-slot-grid.png",
)


def should_exclude(rel_path: str) -> bool:
    normalized = rel_path.replace("\\", "/")
    return any(fnmatch.fnmatch(normalized, pattern) for pattern in EXCLUDE_GLOBS)


def list_tracked_files(root: pathlib.Path) -> list[pathlib.Path]:
    paths = []
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(root).as_posix()
        if should_exclude(rel):
            continue
        paths.append(path)
    return sorted(paths)


def add_member(
    zf: zipfile.ZipFile,
    *,
    source_path: pathlib.Path,
    archive_path: str,
) -> None:
    if source_path.is_symlink():
        info = zipfile.ZipInfo(archive_path, (1980, 1, 1, 0, 0, 0))
        info.create_system = 3 # Unix
        info.external_attr = (0o120000 | 0o777) << 16 # Symlink
        zf.writestr(info, source_path.readlink().as_posix())
        return

    # Construct ZipInfo directly to avoid 'timestamp before 1980' crashes caused by deterministic mtimes
    info = zipfile.ZipInfo(archive_path, (1980, 1, 1, 0, 0, 0))
    info.create_system = 3 # Unix
    info.external_attr = (source_path.stat().st_mode & 0xFFFF) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    with source_path.open("rb") as handle:
        zf.writestr(info, handle.read())


def create_archive(*, root: pathlib.Path, output: pathlib.Path, version: str) -> None:
    files = list_tracked_files(root)
    prefix = f"{root.name}-{version.strip()}/"

    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        output.unlink()

    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as zf:
        for source_path in files:
            rel = source_path.relative_to(root).as_posix()
            add_member(zf, source_path=source_path, archive_path=f"{prefix}{rel}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Create deterministic release source archive")
    parser.add_argument("--root", required=True, help="Repository root")
    parser.add_argument("--version", required=True, help="Release version/tag")
    parser.add_argument("--output", required=True, help="Destination .zip path")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    output = pathlib.Path(args.output).resolve()
    create_archive(root=root, output=output, version=args.version)


if __name__ == "__main__":
    main()
