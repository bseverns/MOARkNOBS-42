#!/usr/bin/env python3
"""Emit build flags defining FW_VERSION and GIT_SHA.

The firmware headers stringize these tokens, so we don't bother with quotes
here. Passing plain identifiers keeps the build flags simple and avoids
escaping headaches on Windows and friends.
"""
import os
import subprocess


def get_git_sha() -> str:
    """Return the current commit SHA or 'unknown' if Git misbehaves."""
    try:
        return (
            subprocess.check_output(["git", "rev-parse", "--short", "HEAD"])  # nosec B603
            .decode()
            .strip()
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def main() -> None:
    fw = os.environ.get("FW_VERSION", "0.0.0")
    sha = get_git_sha()
    # Emit raw tokens; the firmware will turn them into string literals.
    print(f"-DFW_VERSION={fw} -DGIT_SHA={sha}")


if __name__ == "__main__":
    main()

