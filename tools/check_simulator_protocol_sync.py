#!/usr/bin/env python3
"""Keep the App simulator's declared command vocabulary aligned with firmware."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys


def parse_js_object(text: str, name: str) -> dict[str, str]:
    match = re.search(
        rf"export const {re.escape(name)} = Object\.freeze\(\{{(?P<body>.*?)\}}\);",
        text,
        re.DOTALL,
    )
    if not match:
        raise ValueError(f"missing {name} declaration")
    return dict(re.findall(r"^\s*([a-z_]+):\s*'([A-Z_]+)'", match.group("body"), re.MULTILINE))


def parse_js_array(text: str, name: str) -> set[str]:
    match = re.search(
        rf"export const {re.escape(name)} = Object\.freeze\(\[(?P<body>.*?)\]\);",
        text,
        re.DOTALL,
    )
    if not match:
        raise ValueError(f"missing {name} declaration")
    return set(re.findall(r"'([A-Za-z_]+)'", match.group("body")))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=".", help="Repository root")
    args = parser.parse_args()
    root = pathlib.Path(args.root).resolve()

    simulator = (root / "App/runtime/simulator_transport.js").read_text(encoding="utf-8")
    dispatch = (root / "firmware/src/protocol/ProtocolDispatch.cpp").read_text(encoding="utf-8")
    scenes = (root / "firmware/src/protocol/SceneCommands.cpp").read_text(encoding="utf-8")

    native_commands = set(re.findall(r'\{"([A-Z_]+)",\s*ProtocolDispatchHandlers::', dispatch))
    rpc_mapping = parse_js_object(simulator, "SIMULATOR_FIRMWARE_COMMANDS")
    macro_commands = parse_js_array(simulator, "SIMULATOR_MACRO_COMMANDS")
    scene_commands = parse_js_array(simulator, "SIMULATOR_SCENE_COMMANDS")
    simulator_only = parse_js_array(simulator, "SIMULATOR_ONLY_RPCS")
    rpc_cases = set(re.findall(r"case '([a-z_]+)':", simulator))

    errors: list[str] = []
    unmapped_rpcs = rpc_cases - set(rpc_mapping) - simulator_only
    if unmapped_rpcs:
        errors.append(f"unmapped simulator RPC cases: {', '.join(sorted(unmapped_rpcs))}")

    unsupported_native = set(rpc_mapping.values()) | macro_commands
    missing_native = unsupported_native - native_commands
    if missing_native:
        errors.append(f"simulator commands missing from firmware dispatch: {', '.join(sorted(missing_native))}")

    missing_scene = {
        command for command in scene_commands if f'"{command}"' not in scenes
    }
    if missing_scene:
        errors.append(f"simulator scene commands missing from firmware scene handler: {', '.join(sorted(missing_scene))}")

    if errors:
        print("simulator protocol sync check failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        raise SystemExit(1)
    print(
        "simulator protocol sync check passed "
        f"({len(rpc_mapping)} RPC mappings, {len(macro_commands)} macro commands, "
        f"{len(scene_commands)} scene commands)"
    )


if __name__ == "__main__":
    main()
