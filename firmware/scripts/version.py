#!/usr/bin/env python3
"""Emit build flags defining FW_VERSION and GIT_SHA."""
import os, subprocess

def get_git_sha():
    return subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).decode().strip()

def main():
    fw = os.environ.get("FW_VERSION", "0.0.0")
    sha = get_git_sha()
    print(f'-DFW_VERSION=\"{fw}\" -DGIT_SHA=\"{sha}\"')

if __name__ == "__main__":
    main()
