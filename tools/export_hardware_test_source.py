#!/usr/bin/env python3
"""Create a hardware-test source archive from the repository."""

from __future__ import annotations

import argparse
import fnmatch
import pathlib
import zipfile


INCLUDE_GLOBS = (
    ".github/workflows/hardware-test-package.yml",
    "App/**",
    "bridge/**",
    "docs/hardware-test/**",
    "firmware/**",
    "hardware/**",
    "LICENSE",
    "LICENSES/**",
    "README.md",
    "HARDWARE_TEST_README.md",
    "requirements.txt",
    "test.sh",
    "platformio.ini",
    "tools/capture_bench_log.py",
    "tools/check_markdown_links.py",
    "tools/export_hardware_test_source.py",
    "tools/rtl_latency_report.py",
    "tools/run_bench_capture.sh",
    "tools/summarize_bench_log.py",
)

EXCLUDE_GLOBS = (
    ".git/**",
    ".venv/**",
    ".pio/**",
    ".pio-home/**",
    ".pio-cache/**",
    ".platformio/**",
    "firmware/.pio/**",
    "firmware/.venv/**",
    "firmware/.vscode/**",
    "App/node_modules/**",
    "App/test-results/**",
    "App/playwright-report/**",
    "bridge/node_modules/**",
    "logs/**",
    "site/**",
    "dist/**",
    "**/.DS_Store",
    "**/__pycache__/**",
    "**/.pytest_cache/**",
    "tmp-slot-grid.png",
    "**/tmp-slot-grid.png",
)


def _matches(path: str, patterns: tuple[str, ...]) -> bool:
    return any(fnmatch.fnmatch(path, pattern) for pattern in patterns)


def should_include(rel_path: str) -> bool:
    normalized = rel_path.replace("\\", "/")
    return _matches(normalized, INCLUDE_GLOBS) and not _matches(normalized, EXCLUDE_GLOBS)


def list_members(root: pathlib.Path) -> list[pathlib.Path]:
    members = []
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(root).as_posix()
        if should_include(rel):
            members.append(path)
    return sorted(members)


def add_member(zf: zipfile.ZipFile, source_path: pathlib.Path, archive_path: str) -> None:
    info = zipfile.ZipInfo(archive_path, (1980, 1, 1, 0, 0, 0))
    info.create_system = 3
    info.external_attr = (source_path.stat().st_mode & 0xFFFF) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    with source_path.open("rb") as handle:
        zf.writestr(info, handle.read())


def create_archive(root: pathlib.Path, output: pathlib.Path, label: str) -> None:
    members = list_members(root)
    prefix = f"{root.name}-{label.strip()}/"

    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        output.unlink()

    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as zf:
        for source_path in members:
            rel = source_path.relative_to(root).as_posix()
            add_member(zf, source_path, f"{prefix}{rel}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Create a hardware-test source archive")
    parser.add_argument("--root", required=True, help="Repository root")
    parser.add_argument("--label", required=True, help="Archive label")
    parser.add_argument("--output", required=True, help="Destination zip path")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    output = pathlib.Path(args.output).resolve()
    create_archive(root, output, args.label)


if __name__ == "__main__":
    main()
