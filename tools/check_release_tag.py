#!/usr/bin/env python3
"""Validate the semantic, annotated-tag, and changelog release contract."""

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess


SEMVER_TAG = re.compile(r"^v\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$")


def git(root: pathlib.Path, *args: str) -> str:
    completed = subprocess.run(
        ["git", *args],
        cwd=root,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return completed.stdout.strip()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=".", help="Repository root")
    parser.add_argument("--tag", required=True, help="Release tag to validate")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    tag = args.tag.strip()
    errors: list[str] = []

    if not SEMVER_TAG.fullmatch(tag):
        errors.append(f"release tag must use semantic form vX.Y.Z (received {tag!r})")

    try:
        object_type = git(root, "cat-file", "-t", f"refs/tags/{tag}")
        if object_type != "tag":
            errors.append(f"release tag {tag!r} must be annotated (found {object_type!r})")
        release_sha = git(root, "rev-list", "-n", "1", tag)
        checkout_sha = git(root, "rev-parse", "HEAD")
        if release_sha != checkout_sha:
            errors.append(
                f"checkout HEAD {checkout_sha} does not match {tag!r} commit {release_sha}"
            )
    except subprocess.CalledProcessError:
        errors.append(f"release tag {tag!r} does not exist in this checkout")

    changelog = (root / "CHANGELOG.md").read_text(encoding="utf-8")
    heading = re.compile(rf"^## \[{re.escape(tag)}\] - \d{{4}}-\d{{2}}-\d{{2}}$", re.MULTILINE)
    if not heading.search(changelog):
        errors.append(f"CHANGELOG.md must contain '## [{tag}] - YYYY-MM-DD'")

    if errors:
        print("release tag contract failed:")
        for error in errors:
            print(f"  - {error}")
        raise SystemExit(1)

    print(f"release tag contract passed: {tag} -> {release_sha}")


if __name__ == "__main__":
    main()
