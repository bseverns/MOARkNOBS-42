#!/usr/bin/env python3
"""Create a deterministic ZIP from one release staging directory."""

from __future__ import annotations

import argparse
import pathlib
import zipfile


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, help="Directory to archive")
    parser.add_argument("--output", required=True, help="ZIP destination")
    parser.add_argument("--prefix", required=True, help="Top-level directory inside the ZIP")
    args = parser.parse_args()

    source = pathlib.Path(args.source).resolve()
    output = pathlib.Path(args.output).resolve()
    if not source.is_dir():
        raise NotADirectoryError(f"release staging directory not found: {source}")

    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        output.unlink()

    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(source.rglob("*")):
            if not path.is_file():
                continue
            relative = path.relative_to(source)
            info = zipfile.ZipInfo((pathlib.Path(args.prefix) / relative).as_posix())
            info.date_time = (1980, 1, 1, 0, 0, 0)
            info.compress_type = zipfile.ZIP_DEFLATED
            mode = 0o100755 if path.stat().st_mode & 0o111 else 0o100644
            info.external_attr = mode << 16
            archive.writestr(info, path.read_bytes())

    print(f"Archived {source} -> {output}")


if __name__ == "__main__":
    main()
