#!/usr/bin/env python3
"""Lightweight environment and repo-contract checks for contributors."""

from __future__ import annotations

import argparse
import pathlib
import re
import shutil
import subprocess
import sys


def run_capture(cmd: list[str], cwd: pathlib.Path | None = None) -> tuple[int, str, str]:
    completed = subprocess.run(
        cmd,
        cwd=str(cwd) if cwd else None,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return completed.returncode, completed.stdout.strip(), completed.stderr.strip()


def require_file(path: pathlib.Path, errors: list[str], label: str) -> None:
    if not path.exists():
        errors.append(f"missing {label}: {path}")


def parse_node_major(version: str) -> int | None:
    match = re.match(r"^v(\d+)\.", version.strip())
    if not match:
        return None
    return int(match.group(1))


def check_versions(errors: list[str], notes: list[str]) -> None:
    python_ok = sys.version_info >= (3, 11)
    if not python_ok:
        errors.append(
            f"python >= 3.11 required (found {sys.version_info.major}.{sys.version_info.minor})"
        )
    else:
        notes.append(f"python: {sys.version.split()[0]}")

    pio = shutil.which("pio")
    if not pio:
        errors.append("PlatformIO CLI (`pio`) not found in PATH")
    else:
        code, out, err = run_capture(["pio", "--version"])
        if code != 0:
            errors.append(f"failed to run `pio --version`: {err or out}")
        else:
            notes.append(f"pio: {out}")

    node = shutil.which("node")
    if not node:
        errors.append("Node.js (`node`) not found in PATH")
    else:
        code, out, err = run_capture(["node", "--version"])
        if code != 0:
            errors.append(f"failed to run `node --version`: {err or out}")
        else:
            node_major = parse_node_major(out)
            if node_major != 22:
                errors.append(f"Node.js 22.x required for bridge/app lanes (found {out})")
            else:
                notes.append(f"node: {out}")

    npm = shutil.which("npm")
    if not npm:
        errors.append("npm not found in PATH")
    else:
        code, out, err = run_capture(["npm", "--version"])
        if code != 0:
            errors.append(f"failed to run `npm --version`: {err or out}")
        else:
            notes.append(f"npm: {out}")


def check_repo_contract(root: pathlib.Path, errors: list[str], notes: list[str]) -> None:
    root_pio = root / "platformio.ini"
    firmware_pio = root / "firmware" / "platformio.ini"
    require_file(root_pio, errors, "root platformio.ini")
    require_file(firmware_pio, errors, "firmware platformio.ini")

    root_pio_text = root_pio.read_text(encoding="utf-8") if root_pio.exists() else ""
    if "firmware_project_only" not in root_pio_text:
        errors.append("root platformio.ini is expected to block root invocations")
    else:
        notes.append("root PlatformIO guard: enabled")

    require_file(root / "firmware" / "test" / "unity_output.cpp", errors, "custom unity output")
    require_file(root / "firmware" / "test" / "unittest_transport.cpp", errors, "unity transport shim")
    require_file(root / "tools" / "check_contract_sync.py", errors, "contract sync check")
    require_file(
        root / "tools" / "check_schema_keyword_coverage.py",
        errors,
        "schema keyword coverage check",
    )


def check_playwright(root: pathlib.Path, errors: list[str], notes: list[str]) -> None:
    app_dir = root / "App"
    playwright_cli = app_dir / "node_modules" / ".bin" / "playwright"
    if not playwright_cli.exists():
        errors.append(
            "Playwright CLI not found. Run `npm --prefix App ci` and "
            "`npx --prefix App playwright install --with-deps chromium`."
        )
        return

    code, out, err = run_capture(["npx", "--prefix", "App", "playwright", "--version"], cwd=root)
    if code != 0:
        errors.append(f"failed to run Playwright version check: {err or out}")
    else:
        notes.append(f"playwright: {out}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Run local MOARkNOBS-42 contributor health checks.")
    parser.add_argument("--root", default=".", help="Repository root")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    errors: list[str] = []
    notes: list[str] = []

    check_versions(errors, notes)
    check_repo_contract(root, errors, notes)
    check_playwright(root, errors, notes)

    if notes:
        print("Doctor checks:")
        for note in notes:
            print(f"  - {note}")

    if errors:
        print("\nDoctor found issues:")
        for err in errors:
            print(f"  - {err}")
        raise SystemExit(1)

    print("\nDoctor check passed.")


if __name__ == "__main__":
    main()
