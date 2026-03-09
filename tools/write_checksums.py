#!/usr/bin/env python3
"""Write deterministic SHA-256 checksum files."""

from __future__ import annotations

import argparse
import hashlib
import pathlib


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description="Write SHA-256 checksums for artifact files.")
    parser.add_argument("--output", required=True, help="Checksum file destination path")
    parser.add_argument("--file", action="append", default=[], help="Artifact file path to checksum")
    args = parser.parse_args()

    if not args.file:
        parser.error("At least one --file is required")

    output_path = pathlib.Path(args.output).resolve()
    files = sorted(pathlib.Path(item).resolve() for item in args.file)

    lines = []
    for file_path in files:
        if not file_path.is_file():
            raise FileNotFoundError(f"missing artifact: {file_path}")
        lines.append(f"{sha256(file_path)}  {file_path.name}")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
