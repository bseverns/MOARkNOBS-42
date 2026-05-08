#!/usr/bin/env python3
"""Ensure app fallback manifest/schema stay aligned with firmware contract."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys


def parse_int_constant(pattern: str, text: str, *, base: int = 10) -> int:
    match = re.search(pattern, text)
    if not match:
        raise ValueError(f"missing constant for pattern: {pattern}")
    return int(match.group(1), base)


def parse_string_constant(pattern: str, text: str) -> str:
    match = re.search(pattern, text)
    if not match:
        raise ValueError(f"missing constant for pattern: {pattern}")
    return match.group(1)


def parse_js_int_constant(name: str, text: str) -> int:
    return parse_int_constant(rf"export const {re.escape(name)} = (\d+);", text, base=10)


def parse_js_str_constant(name: str, text: str) -> str:
    return parse_string_constant(rf"export const {re.escape(name)} = '([^']+)';", text)


def parse_js_bool_constant(name: str, text: str) -> bool:
    match = re.search(rf"export const {re.escape(name)} = (true|false);", text)
    if not match:
        raise ValueError(f"missing boolean constant: {name}")
    return match.group(1) == "true"


def parse_bridge_literal(key: str, text: str):
    pattern = rf"{re.escape(key)}:\s*('([^']+)'|(\d+)|(true|false)),"
    match = re.search(pattern, text)
    if not match:
        raise ValueError(f"missing bridge contract key: {key}")
    if match.group(2) is not None:
        return match.group(2)
    if match.group(3) is not None:
        return int(match.group(3))
    return match.group(4) == "true"


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate app/firmware contract alignment.")
    parser.add_argument("--root", default=".", help="Repository root")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    globals_h = (root / "firmware/include/Globals.h").read_text(encoding="utf-8")
    globals_cpp = (root / "firmware/src/Globals.cpp").read_text(encoding="utf-8")
    midi_types_h = (root / "firmware/include/MIDITypes.h").read_text(encoding="utf-8")
    manifest_contract_h = (root / "firmware/include/protocol/ManifestContract.h").read_text(
        encoding="utf-8"
    )
    board_power_h = (root / "firmware/include/BoardPowerProfile.h").read_text(encoding="utf-8")
    app_contract_js = (root / "App/manifest_contract.js").read_text(encoding="utf-8")
    bridge_contract_js = (root / "bridge/lib/manifest_contract.js").read_text(encoding="utf-8")
    app_schema = json.loads((root / "App/config_schema.json").read_text(encoding="utf-8"))

    slot_led_count = parse_int_constant(r"\.slotLedCount = (\d+),", globals_cpp)
    ef_led_count = parse_int_constant(r"\.efLedCount = (\d+),", globals_cpp)
    pot_led_count = parse_int_constant(r"\.potLedCount = (\d+),", globals_cpp)
    derived_led_count = slot_led_count + ef_led_count + 1 + pot_led_count
    power_choked_block = re.search(
        r"#if MN42_BOARD_POWER_PROFILE == POWER_CHOKED_V1(?P<body>.*?)#elif",
        board_power_h,
        re.DOTALL,
    )
    if not power_choked_block:
        raise ValueError("missing POWER_CHOKED_V1 profile block")
    power_choked = power_choked_block.group("body")

    firmware = {
        "schema_version": parse_int_constant(r"CONFIG_VERSION = 0x([0-9A-Fa-f]+);", globals_h, base=16),
        "slot_count": parse_int_constant(r"NUM_SLOTS = (\d+);", midi_types_h),
        "pot_count": parse_int_constant(r"NUM_POTS = (\d+);", globals_h),
        "envelope_count": parse_int_constant(r"NUM_ENVELOPES = (\d+);", globals_h),
        "device_name": parse_string_constant(r'kDeviceName\[\] = "([^"]+)";', manifest_contract_h),
        "led_count": parse_int_constant(r"kDefaultLedCount = (\d+);", manifest_contract_h),
        "power_profile": parse_string_constant(
            r'MN42_BOARD_POWER_PROFILE_NAME "([^"]+)"', power_choked
        ),
        "led_brightness_cap": parse_int_constant(
            r"MN42_LED_BRIGHTNESS_CAP_VALUE (\d+)", power_choked
        ),
        "rail_topology_verified": parse_string_constant(
            r"MN42_BOARD_POWER_PROFILE_RAIL_TOPOLOGY_VERIFIED (true|false)", power_choked
        )
        == "true",
    }

    app = {
        "schema_version": parse_js_int_constant("MN42_SCHEMA_VERSION", app_contract_js),
        "slot_count": parse_js_int_constant("MN42_SLOT_COUNT", app_contract_js),
        "pot_count": parse_js_int_constant("MN42_POT_COUNT", app_contract_js),
        "envelope_count": parse_js_int_constant("MN42_ENVELOPE_COUNT", app_contract_js),
        "device_name": parse_js_str_constant("MN42_DEVICE_NAME", app_contract_js),
        "led_count": parse_js_int_constant("MN42_LED_COUNT", app_contract_js),
        "power_profile": parse_js_str_constant("MN42_POWER_PROFILE", app_contract_js),
        "led_brightness_cap": parse_js_int_constant("MN42_LED_BRIGHTNESS_CAP", app_contract_js),
        "rail_topology_verified": parse_js_bool_constant(
            "MN42_RAIL_TOPOLOGY_VERIFIED", app_contract_js
        ),
    }

    bridge = {
        key: parse_bridge_literal(key, bridge_contract_js)
        for key in (
            "schema_version",
            "slot_count",
            "pot_count",
            "envelope_count",
            "device_name",
            "led_count",
            "power_profile",
            "led_brightness_cap",
            "rail_topology_verified",
        )
    }

    errors: list[str] = []
    if firmware["led_count"] != derived_led_count:
        errors.append(
            "led_count: ManifestContract "
            f"{firmware['led_count']!r} != Globals-derived {derived_led_count!r}"
        )

    for key in (
        "schema_version",
        "slot_count",
        "pot_count",
        "envelope_count",
        "device_name",
        "led_count",
        "power_profile",
        "led_brightness_cap",
        "rail_topology_verified",
    ):
        if firmware[key] != app[key]:
            errors.append(f"{key}: firmware={firmware[key]!r} app={app[key]!r}")
        if firmware[key] != bridge[key]:
            errors.append(f"{key}: firmware={firmware[key]!r} bridge={bridge[key]!r}")

    schema_version = app_schema.get("schema_version")
    if schema_version != app["schema_version"]:
        errors.append(
            "App/config_schema.json schema_version "
            f"{schema_version!r} != App/manifest_contract.js {app['schema_version']!r}"
        )

    if errors:
        for err in errors:
            print(err)
        print(f"\ncontract sync check failed: {len(errors)} mismatch(es)", file=sys.stderr)
        raise SystemExit(1)

    print("contract sync check passed")


if __name__ == "__main__":
    main()
