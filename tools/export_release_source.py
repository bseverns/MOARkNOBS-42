#!/usr/bin/env python3
"""Create a deterministic source export for release uploads."""

from __future__ import annotations

import argparse
import fnmatch
import gzip
import pathlib
import subprocess
import tarfile


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
    tar: tarfile.TarFile,
    *,
    source_path: pathlib.Path,
    archive_path: str,
) -> None:
    if source_path.is_symlink():
        info = tarfile.TarInfo(archive_path)
        info.type = tarfile.SYMTYPE
        info.linkname = source_path.readlink().as_posix()
        info.mtime = 0
        info.uid = 0
        info.gid = 0
        info.uname = ""
        info.gname = ""
        info.mode = 0o777
        tar.addfile(info)
        return

    info = tar.gettarinfo(str(source_path), arcname=archive_path)
    info.mtime = 0
    info.uid = 0
    info.gid = 0
    info.uname = ""
    info.gname = ""
    info.mode = 0o755 if source_path.stat().st_mode & 0o111 else 0o644
    with source_path.open("rb") as handle:
        tar.addfile(info, handle)


def create_archive(*, root: pathlib.Path, output: pathlib.Path, version: str) -> None:
    files = list_tracked_files(root)
    prefix = f"{root.name}-{version.strip()}/"

    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        output.unlink()

    with output.open("wb") as raw_handle:
        with gzip.GzipFile(fileobj=raw_handle, mode="wb", mtime=0) as gzip_handle:
            with tarfile.open(fileobj=gzip_handle, mode="w", format=tarfile.PAX_FORMAT) as tar:
                for source_path in files:
                    rel = source_path.relative_to(root).as_posix()
                    add_member(tar, source_path=source_path, archive_path=f"{prefix}{rel}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Create deterministic release source archive")
    parser.add_argument("--root", required=True, help="Repository root")
    parser.add_argument("--version", required=True, help="Release version/tag")
    parser.add_argument("--output", required=True, help="Destination .tar.gz path")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    output = pathlib.Path(args.output).resolve()
    create_archive(root=root, output=output, version=args.version)


if __name__ == "__main__":
    main()
