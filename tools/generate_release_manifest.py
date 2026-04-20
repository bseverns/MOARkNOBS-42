#!/usr/bin/env python3
"""Emit a reproducibility manifest for release artifacts."""

from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import os
import pathlib
import subprocess
import sys
from typing import Dict, Iterable, List


def _run(cmd: List[str], *, cwd: pathlib.Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        cwd=str(cwd) if cwd is not None else None,
    )


def _capture_text(cmd: List[str], *, cwd: pathlib.Path | None = None) -> Dict[str, object]:
    try:
        completed = _run(cmd, cwd=cwd)
    except subprocess.CalledProcessError as exc:  # pragma: no cover - defensive only
        return {
            "command": cmd,
            "returncode": exc.returncode,
            "stdout": exc.stdout.strip(),
            "stderr": exc.stderr.strip(),
        }
    return {
        "command": cmd,
        "stdout": completed.stdout.strip(),
        "stderr": completed.stderr.strip(),
    }


def _file_metadata(path: pathlib.Path, root: pathlib.Path | None) -> Dict[str, object]:
    path = path.resolve()
    data = path.read_bytes()
    info: Dict[str, object] = {
        "path": str(path),
        "size_bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
    }
    if root is not None:
        try:
            info["relative_path"] = str(path.relative_to(root))
        except ValueError:
            pass
    return info


def _git_info(root: pathlib.Path) -> Dict[str, object]:
    def run_git(args: Iterable[str]) -> str:
        completed = _run(["git", *args], cwd=root)
        return completed.stdout.strip()

    try:
        info: Dict[str, object] = {
            "commit": run_git(["rev-parse", "HEAD"]),
            "describe": run_git(["describe", "--tags", "--always"]),
            "branch": run_git(["rev-parse", "--abbrev-ref", "HEAD"]),
        }
        status = _run(["git", "status", "--short"], cwd=root)
        info["dirty"] = bool(status.stdout.strip())
        if info["dirty"]:
            info["status"] = status.stdout.strip()
        return info
    except Exception as exc:
        return {"error": "Not a valid git repository or git not installed", "details": str(exc)}


def _collect_env(prefix: str) -> Dict[str, str]:
    return {k: v for k, v in sorted(os.environ.items()) if k.startswith(prefix)}


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a deterministic build manifest")
    parser.add_argument("--version", required=True, help="Release version/tag")
    parser.add_argument("--output", required=True, help="Manifest destination path")
    parser.add_argument("--root", required=False, help="Repository root for relative paths")
    parser.add_argument("--project", required=True, help="PlatformIO project directory")
    parser.add_argument("--build-env", required=True, help="PlatformIO environment used for the firmware build")
    parser.add_argument("--firmware", required=True, help="Path to the compiled Intel HEX firmware artifact")
    parser.add_argument("--fabrication", required=True, help="Path to the hardware reference bundle artifact")
    parser.add_argument(
        "--artifact",
        action="append",
        default=[],
        help="Additional artifact metadata as label=path",
    )
    parser.add_argument(
        "--verification-file",
        required=False,
        help="Optional JSON summary emitted by release verification lane",
    )
    parser.add_argument("--pio-home", required=False, help="Explicit PlatformIO home directory")
    parser.add_argument("--test", action="append", default=[], help="Test status entries as name=status")
    parser.add_argument("--step", action="append", default=[], help="Command steps recorded as label=command")

    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve() if args.root else None
    project_dir = pathlib.Path(args.project).resolve()
    firmware_path = pathlib.Path(args.firmware)
    fabrication_path = pathlib.Path(args.fabrication)

    now = _dt.datetime.now(tz=_dt.timezone.utc)

    tests: Dict[str, Dict[str, str]] = {}
    for entry in args.test:
        if "=" not in entry:
            parser.error(f"Invalid --test value '{entry}'. Expected name=status.")
        name, status = entry.split("=", 1)
        tests[name.strip()] = {"status": status.strip()}

    verification_data: Dict[str, object] | None = None
    if args.verification_file:
        verification_path = pathlib.Path(args.verification_file).resolve()
        verification_data = json.loads(verification_path.read_text(encoding="utf-8"))
        verification_tests = verification_data.get("tests")
        if isinstance(verification_tests, dict):
            for name, entry in verification_tests.items():
                if name in tests:
                    continue
                if isinstance(entry, dict) and entry.get("status") is not None:
                    tests[str(name)] = {"status": str(entry["status"])}
                elif entry is not None:
                    tests[str(name)] = {"status": str(entry)}

    steps: List[Dict[str, str]] = []
    for entry in args.step:
        if "=" not in entry:
            parser.error(f"Invalid --step value '{entry}'. Expected label=command.")
        label, command = entry.split("=", 1)
        steps.append({"label": label.strip(), "command": command.strip()})

    extra_artifacts: Dict[str, pathlib.Path] = {}
    for entry in args.artifact:
        if "=" not in entry:
            parser.error(f"Invalid --artifact value '{entry}'. Expected label=path.")
        label, artifact_path = entry.split("=", 1)
        clean_label = label.strip()
        if not clean_label:
            parser.error(f"Invalid --artifact label in '{entry}'.")
        extra_artifacts[clean_label] = pathlib.Path(artifact_path.strip())

    try:
        pio_info_output = _run(["pio", "system", "info", "--json-output"], cwd=project_dir).stdout.strip()
        try:
            pio_info = json.loads(pio_info_output)
        except json.JSONDecodeError:
            pio_info = {"raw": pio_info_output}
    except subprocess.CalledProcessError as exc:  # pragma: no cover - defensive only
        pio_info = {
            "error": exc.stderr.strip() or exc.stdout.strip(),
            "returncode": exc.returncode,
        }

    pkg_inventory = _capture_text(["pio", "pkg", "list", "-d", str(project_dir), "-e", args.build_env], cwd=project_dir)

    repo_dir = root if root is not None else project_dir

    artifacts: Dict[str, object] = {
        "firmware_hex": _file_metadata(firmware_path, root),
        "hardware_reference_bundle": _file_metadata(fabrication_path, root),
    }
    for label, artifact_path in extra_artifacts.items():
        artifacts[label] = _file_metadata(artifact_path, root)

    manifest: Dict[str, object] = {
        "version": args.version,
        "generated_at_utc": now.isoformat(),
        "python": {
            "executable": sys.executable,
            "version": sys.version,
        },
        "git": _git_info(repo_dir),
        "platformio": {
            "system_info": pio_info,
            "home": args.pio_home,
            "project_dir": str(project_dir),
            "environment": args.build_env,
            "package_list": pkg_inventory,
        },
        "environment_variables": {
            "platformio": _collect_env("PLATFORMIO_"),
        },
        "commands": steps,
        "tests": tests,
        "artifacts": artifacts,
    }

    if verification_data is not None:
        manifest["verification"] = verification_data

    if root is not None:
        manifest["root"] = str(root)

    output_path = pathlib.Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8") as fh:
        json.dump(manifest, fh, indent=2, sort_keys=True)
        fh.write("\n")


if __name__ == "__main__":
    main()
