#!/usr/bin/env python3
"""Ensure host fallbacks and shared schema semantics stay aligned with firmware."""

from __future__ import annotations

import argparse
import ast
import json
import pathlib
import re
import sys
from typing import Any

SUPPORTED_VALIDATION_KEYWORDS = {
    "type",
    "enum",
    "minimum",
    "maximum",
    "maxLength",
    "pattern",
    "required",
    "additionalProperties",
    "properties",
    "anyOf",
    "minItems",
    "maxItems",
    "items",
    "uniqueItems",
}

NON_VALIDATION_SCHEMA_KEYWORDS = {
    "$schema",
    "title",
    "description",
    "default",
    "schema_version",
}


def parse_int_constant(pattern: str, text: str, *, base: int = 10) -> int:
    match = re.search(pattern, text)
    if not match:
        raise ValueError(f"missing constant for pattern: {pattern}")
    return int(match.group(1), base)


def parse_float_constant(pattern: str, text: str) -> float:
    match = re.search(pattern, text)
    if not match:
        raise ValueError(f"missing float constant for pattern: {pattern}")
    return float(match.group(1))


def parse_string_constant(pattern: str, text: str) -> str:
    match = re.search(pattern, text)
    if not match:
        raise ValueError(f"missing constant for pattern: {pattern}")
    return match.group(1)


def parse_cpp_enum_index(text: str, enum_name: str, member_name: str) -> int:
    block = re.search(rf"enum class {re.escape(enum_name)}\s*:\s*\w+\s*\{{(?P<body>.*?)\}};", text, re.DOTALL)
    if not block:
        raise ValueError(f"missing enum block: {enum_name}")

    body = block.group("body")
    members = []
    for raw_entry in body.split(","):
        entry = raw_entry.strip()
        if not entry:
            continue
        name = entry.split("=")[0].strip()
        if not name:
            continue
        members.append(name)

    if member_name not in members:
        raise ValueError(f"missing enum member: {enum_name}::{member_name}")
    return members.index(member_name)


def parse_js_int_constant(name: str, text: str) -> int:
    return parse_int_constant(rf"export const {re.escape(name)} = (\d+);", text, base=10)


def parse_js_str_constant(name: str, text: str) -> str:
    return parse_string_constant(rf"export const {re.escape(name)} = '([^']+)';", text)


def parse_js_bool_constant(name: str, text: str) -> bool:
    match = re.search(rf"export const {re.escape(name)} = (true|false);", text)
    if not match:
        raise ValueError(f"missing boolean constant: {name}")
    return match.group(1) == "true"


def parse_bridge_literal(key: str, text: str) -> Any:
    pattern = rf"{re.escape(key)}:\s*('([^']+)'|(\d+)|(true|false)),"
    match = re.search(pattern, text)
    if not match:
        raise ValueError(f"missing bridge contract key: {key}")
    if match.group(2) is not None:
        return match.group(2)
    if match.group(3) is not None:
        return int(match.group(3))
    return match.group(4) == "true"


def split_top_level_commas(text: str) -> list[str]:
    parts: list[str] = []
    depth = 0
    start = 0
    for idx, char in enumerate(text):
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
        elif char == "," and depth == 0:
            parts.append(text[start:idx].strip())
            start = idx + 1
    parts.append(text[start:].strip())
    return parts


def strip_static_casts(expr: str) -> str:
    pattern = re.compile(r"static_cast<[^>]+>\(([^()]+)\)")
    stripped = expr
    while True:
        updated = pattern.sub(r"\1", stripped)
        if updated == stripped:
            return stripped
        stripped = updated


def evaluate_cpp_expression(expr: str, constants: dict[str, float | int]) -> float | int:
    normalized = strip_static_casts(expr.strip())
    for name in sorted(constants, key=len, reverse=True):
        normalized = normalized.replace(name, repr(constants[name]))

    if not re.fullmatch(r"[0-9eE\.\+\-\*/\(\) ]+", normalized):
        raise ValueError(f"unsupported schema expression: {expr!r} -> {normalized!r}")

    value = eval(normalized, {"__builtins__": {}}, {})
    if not isinstance(value, (int, float)):
        raise ValueError(f"unexpected schema value type for {expr!r}: {type(value).__name__}")
    return value


def render_arduino_string(value: float | int, precision: int | None) -> str:
    if precision is not None:
        return f"{float(value):.{precision}f}"
    if isinstance(value, float) and value.is_integer():
        return str(int(value))
    return str(value)


def decode_cpp_string_literals(expr: str) -> str:
    tokens = re.findall(r'"(?:\\.|[^"\\])*"', expr, re.DOTALL)
    if not tokens:
        raise ValueError(f"unable to decode schema string literal: {expr!r}")

    remainder = expr
    for token in tokens:
        remainder = remainder.replace(token, "", 1)
    if remainder.strip():
        raise ValueError(f"unexpected trailing schema literal content: {expr!r}")

    return "".join(ast.literal_eval(token) for token in tokens)


def evaluate_schema_append(expr: str, constants: dict[str, float | int]) -> str:
    rhs = expr.strip()
    string_match = re.fullmatch(r"String\((.*)\)", rhs, re.DOTALL)
    if string_match:
        args = split_top_level_commas(string_match.group(1))
        if not args:
            raise ValueError(f"invalid String() append: {expr!r}")
        value = evaluate_cpp_expression(args[0], constants)
        precision = int(args[1]) if len(args) > 1 else None
        return render_arduino_string(value, precision)
    return decode_cpp_string_literals(rhs)


def materialize_firmware_schema(config_manager_cpp: str, constants: dict[str, float | int]) -> dict[str, Any]:
    match = re.search(
        r"String ConfigManager::makeSchema\(\)\s*\{(?P<body>.*?)return s;",
        config_manager_cpp,
        re.DOTALL,
    )
    if not match:
        raise ValueError("missing ConfigManager::makeSchema() body")

    body = match.group("body")
    chunks: list[str] = []
    for append in re.finditer(r"s \+= (?P<rhs>.*?);", body, re.DOTALL):
        chunks.append(evaluate_schema_append(append.group("rhs"), constants))

    return json.loads("".join(chunks))


def walk_schema_keywords(
    node: object,
    path: str,
    *,
    used: set[str],
    unknown: list[str],
    allowed_metadata: set[str],
) -> None:
    if not isinstance(node, dict):
        return

    for key, value in node.items():
        if key in SUPPORTED_VALIDATION_KEYWORDS:
            used.add(key)
        elif key not in allowed_metadata:
            unknown.append(f"{path or '#'} -> {key}")

        if key == "properties":
            if isinstance(value, dict):
                for prop_name, prop_schema in value.items():
                    walk_schema_keywords(
                        prop_schema,
                        f"{path}/properties/{prop_name}" if path else f"#/properties/{prop_name}",
                        used=used,
                        unknown=unknown,
                        allowed_metadata=allowed_metadata,
                    )
            continue

        if key == "items":
            walk_schema_keywords(
                value,
                f"{path}/items" if path else "#/items",
                used=used,
                unknown=unknown,
                allowed_metadata=allowed_metadata,
            )
            continue

        if key == "anyOf":
            if isinstance(value, list):
                for idx, candidate in enumerate(value):
                    walk_schema_keywords(
                        candidate,
                        f"{path}/anyOf/{idx}" if path else f"#/anyOf/{idx}",
                        used=used,
                        unknown=unknown,
                        allowed_metadata=allowed_metadata,
                    )
            continue

        if key in {"required", "enum", "default", "description", "title", "$schema", "schema_version"}:
            continue

        if isinstance(value, dict) and key in SUPPORTED_VALIDATION_KEYWORDS:
            for nested_key, nested_val in value.items():
                if isinstance(nested_val, (dict, list)):
                    walk_schema_keywords(
                        nested_val,
                        f"{path}/{key}/{nested_key}" if path else f"#/{key}/{nested_key}",
                        used=used,
                        unknown=unknown,
                        allowed_metadata=allowed_metadata,
                    )
        elif isinstance(value, list) and key in SUPPORTED_VALIDATION_KEYWORDS:
            for idx, nested_val in enumerate(value):
                if isinstance(nested_val, (dict, list)):
                    walk_schema_keywords(
                        nested_val,
                        f"{path}/{key}/{idx}" if path else f"#/{key}/{idx}",
                        used=used,
                        unknown=unknown,
                        allowed_metadata=allowed_metadata,
                    )


def schema_at(schema: dict[str, Any], path: tuple[str, ...]) -> Any:
    value: Any = schema
    for part in path:
        if not isinstance(value, dict) or part not in value:
            raise KeyError("/".join(path))
        value = value[part]
    return value


def compare_schema_path(
    firmware_schema: dict[str, Any],
    app_schema: dict[str, Any],
    path: tuple[str, ...],
    errors: list[str],
) -> None:
    pointer = "#/" + "/".join(path)
    try:
        firmware_value = schema_at(firmware_schema, path)
    except KeyError:
        errors.append(f"{pointer}: missing in firmware-generated schema")
        return

    try:
        app_value = schema_at(app_schema, path)
    except KeyError:
        errors.append(f"{pointer}: missing in App/config_schema.json")
        return

    if firmware_value != app_value:
        errors.append(f"{pointer}: firmware={firmware_value!r} app={app_value!r}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate firmware/app/bridge contract alignment.")
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
    config_manager_cpp = (root / "firmware/src/ConfigManager.cpp").read_text(encoding="utf-8")
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

    schema_constants: dict[str, float | int] = {
        "CONFIG_VERSION": firmware["schema_version"],
        "NUM_SLOTS": firmware["slot_count"],
        "NUM_ENVELOPES": firmware["envelope_count"],
        "ARGMethod::XORR": parse_cpp_enum_index(midi_types_h, "ARGMethod", "XORR"),
        "EF_OVERSAMPLE_MIN": parse_int_constant(r"EF_OVERSAMPLE_MIN = (\d+);", midi_types_h),
        "EF_OVERSAMPLE_MAX": parse_int_constant(r"EF_OVERSAMPLE_MAX = (\d+);", midi_types_h),
        "EF_TIME_MIN_MS": parse_int_constant(r"EF_TIME_MIN_MS = (\d+);", midi_types_h),
        "EF_TIME_MAX_MS": parse_int_constant(r"EF_TIME_MAX_MS = (\d+);", midi_types_h),
        "EF_FILTER_FREQ_MIN_HZ": parse_float_constant(r"EF_FILTER_FREQ_MIN_HZ = ([0-9.]+)f;", midi_types_h),
        "EF_FILTER_FREQ_MAX_HZ": parse_float_constant(r"EF_FILTER_FREQ_MAX_HZ = ([0-9.]+)f;", midi_types_h),
        "EF_FILTER_Q_MIN": parse_float_constant(r"EF_FILTER_Q_MIN = ([0-9.]+)f;", midi_types_h),
        "EF_FILTER_Q_MAX": parse_float_constant(r"EF_FILTER_Q_MAX = ([0-9.]+)f;", midi_types_h),
        "EF_IDLE_FLOOR_DEFAULT": parse_int_constant(r"EF_IDLE_FLOOR_DEFAULT = (\d+);", globals_h),
    }
    firmware_schema = materialize_firmware_schema(config_manager_cpp, schema_constants)

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

    keyword_errors: list[str] = []
    for schema_name, schema, allowed_metadata in (
        ("App/config_schema.json", app_schema, NON_VALIDATION_SCHEMA_KEYWORDS),
        (
            "firmware ConfigManager::makeSchema()",
            firmware_schema,
            NON_VALIDATION_SCHEMA_KEYWORDS | {"x_mn42"},
        ),
    ):
        used_keywords: set[str] = set()
        unknown_keywords: list[str] = []
        walk_schema_keywords(
            schema,
            "#",
            used=used_keywords,
            unknown=unknown_keywords,
            allowed_metadata=allowed_metadata,
        )
        for entry in unknown_keywords:
            keyword_errors.append(f"{schema_name}: unsupported keyword {entry}")

    errors.extend(keyword_errors)

    shared_schema_paths = [
        ("schema_version",),
        ("required",),
        ("additionalProperties",),
        ("properties", "slots", "minItems"),
        ("properties", "slots", "maxItems"),
        ("properties", "slots", "items", "required"),
        ("properties", "slots", "items", "additionalProperties"),
        ("properties", "slots", "items", "properties", "type", "enum"),
        ("properties", "slots", "items", "properties", "midiChannel", "minimum"),
        ("properties", "slots", "items", "properties", "midiChannel", "maximum"),
        ("properties", "slots", "items", "properties", "data1", "minimum"),
        ("properties", "slots", "items", "properties", "data1", "maximum"),
        ("properties", "slots", "items", "properties", "arpNote", "minimum"),
        ("properties", "slots", "items", "properties", "arpNote", "maximum"),
        ("properties", "slots", "items", "properties", "efIndex", "minimum"),
        ("properties", "slots", "items", "properties", "efIndex", "maximum"),
        ("properties", "slots", "items", "properties", "active", "type"),
        ("properties", "slots", "items", "properties", "sysexTemplate", "maxLength"),
        ("properties", "slots", "items", "properties", "ef", "required"),
        ("properties", "slots", "items", "properties", "ef", "additionalProperties"),
        ("properties", "slots", "items", "properties", "ef", "properties", "index", "minimum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "index", "maximum"),
        (
            "properties",
            "slots",
            "items",
            "properties",
            "ef",
            "properties",
            "filter_index",
            "minimum",
        ),
        (
            "properties",
            "slots",
            "items",
            "properties",
            "ef",
            "properties",
            "filter_index",
            "maximum",
        ),
        ("properties", "slots", "items", "properties", "ef", "properties", "filter_name", "enum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "frequency", "minimum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "frequency", "maximum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "q", "minimum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "q", "maximum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "oversample", "minimum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "oversample", "maximum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "smoothing", "minimum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "smoothing", "maximum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "mode", "minimum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "mode", "maximum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "autoBaseline", "type"),
        ("properties", "slots", "items", "properties", "ef", "properties", "autoGain", "type"),
        ("properties", "slots", "items", "properties", "ef", "properties", "attackMs", "minimum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "attackMs", "maximum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "releaseMs", "minimum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "releaseMs", "maximum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "rmsWindowMs", "minimum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "rmsWindowMs", "maximum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "baselineTauMs", "minimum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "baselineTauMs", "maximum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "gainTauMs", "minimum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "gainTauMs", "maximum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "gateThreshold", "minimum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "gateThreshold", "maximum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "gateHysteresis", "minimum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "gateHysteresis", "maximum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "activityThreshold", "minimum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "activityThreshold", "maximum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "gainTarget", "minimum"),
        ("properties", "slots", "items", "properties", "ef", "properties", "gainTarget", "maximum"),
        ("properties", "slots", "items", "properties", "arg", "required"),
        ("properties", "slots", "items", "properties", "arg", "additionalProperties"),
        ("properties", "slots", "items", "properties", "arg", "properties", "method", "minimum"),
        ("properties", "slots", "items", "properties", "arg", "properties", "method", "maximum"),
        ("properties", "slots", "items", "properties", "arg", "properties", "method_name", "enum"),
        ("properties", "slots", "items", "properties", "arg", "properties", "sourceA", "minimum"),
        ("properties", "slots", "items", "properties", "arg", "properties", "sourceA", "maximum"),
        ("properties", "slots", "items", "properties", "arg", "properties", "sourceB", "minimum"),
        ("properties", "slots", "items", "properties", "arg", "properties", "sourceB", "maximum"),
        ("properties", "efSlots", "minItems"),
        ("properties", "efSlots", "maxItems"),
        ("properties", "efSlots", "items", "anyOf"),
        ("properties", "efSlots", "items", "additionalProperties"),
        ("properties", "efSlots", "items", "properties", "slot", "minimum"),
        ("properties", "efSlots", "items", "properties", "slot", "maximum"),
        ("properties", "efSlots", "items", "properties", "slots", "items", "minimum"),
        ("properties", "efSlots", "items", "properties", "slots", "items", "maximum"),
        ("properties", "efSlots", "items", "properties", "slots", "uniqueItems"),
        ("properties", "filter", "required"),
        ("properties", "filter", "additionalProperties"),
        ("properties", "filter", "properties", "type", "enum"),
        ("properties", "filter", "properties", "freq", "minimum"),
        ("properties", "filter", "properties", "freq", "maximum"),
        ("properties", "filter", "properties", "q", "minimum"),
        ("properties", "filter", "properties", "q", "maximum"),
        ("properties", "filter", "properties", "idle_floor", "minimum"),
        ("properties", "filter", "properties", "idle_floor", "maximum"),
        ("properties", "filter", "properties", "idle_floor", "default"),
        ("properties", "arg", "required"),
        ("properties", "arg", "additionalProperties"),
        ("properties", "arg", "properties", "method", "enum"),
        ("properties", "arg", "properties", "a", "minimum"),
        ("properties", "arg", "properties", "a", "maximum"),
        ("properties", "arg", "properties", "b", "minimum"),
        ("properties", "arg", "properties", "b", "maximum"),
        ("properties", "arg", "properties", "enable", "type"),
        ("properties", "led", "required"),
        ("properties", "led", "additionalProperties"),
        ("properties", "led", "properties", "brightness", "minimum"),
        ("properties", "led", "properties", "brightness", "maximum"),
        ("properties", "led", "properties", "color", "pattern"),
        ("properties", "led", "properties", "mode", "enum"),
        ("properties", "led", "properties", "mode", "default"),
        ("properties", "envelopeMode", "enum"),
        ("properties", "envelopeMode", "default"),
    ]
    for path in shared_schema_paths:
        compare_schema_path(firmware_schema, app_schema, path, errors)

    if errors:
        for err in errors:
            print(err)
        print(f"\ncontract sync check failed: {len(errors)} mismatch(es)", file=sys.stderr)
        raise SystemExit(1)

    print("contract sync check passed")


if __name__ == "__main__":
    main()
