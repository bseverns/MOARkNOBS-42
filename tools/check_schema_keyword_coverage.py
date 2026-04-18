#!/usr/bin/env python3
"""Ensure App/config_schema.json uses only validator-supported keywords."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys

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


def walk_schema(node: object, path: str, *, used: set[str], unknown: list[str]) -> None:
    if not isinstance(node, dict):
        return

    for key, value in node.items():
        if key in SUPPORTED_VALIDATION_KEYWORDS:
            used.add(key)
        elif key not in NON_VALIDATION_SCHEMA_KEYWORDS:
            unknown.append(f"{path or '#'} -> {key}")

        if key == "properties":
            if isinstance(value, dict):
                for prop_name, prop_schema in value.items():
                    walk_schema(
                        prop_schema,
                        f"{path}/properties/{prop_name}" if path else f"#/properties/{prop_name}",
                        used=used,
                        unknown=unknown,
                    )
            continue

        if key == "items":
            walk_schema(value, f"{path}/items" if path else "#/items", used=used, unknown=unknown)
            continue

        if key == "anyOf":
            if isinstance(value, list):
                for idx, candidate in enumerate(value):
                    walk_schema(
                        candidate,
                        f"{path}/anyOf/{idx}" if path else f"#/anyOf/{idx}",
                        used=used,
                        unknown=unknown,
                    )
            continue

        # `required`, `enum`, and primitive metadata nodes are data, not nested schemas.
        if key in {"required", "enum", "default", "description", "title", "$schema"}:
            continue

        # Defensive: if new nested schema containers are introduced, this guard
        # forces explicit handling instead of silently skipping.
        if isinstance(value, dict) and key in SUPPORTED_VALIDATION_KEYWORDS:
            for nested_key, nested_val in value.items():
                if isinstance(nested_val, (dict, list)):
                    walk_schema(
                        nested_val,
                        f"{path}/{key}/{nested_key}" if path else f"#/{key}/{nested_key}",
                        used=used,
                        unknown=unknown,
                    )
        elif isinstance(value, list) and key in SUPPORTED_VALIDATION_KEYWORDS:
            for idx, nested_val in enumerate(value):
                if isinstance(nested_val, (dict, list)):
                    walk_schema(
                        nested_val,
                        f"{path}/{key}/{idx}" if path else f"#/{key}/{idx}",
                        used=used,
                        unknown=unknown,
                    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Verify schema keywords remain compatible with App/lib/mini-ajv.js."
    )
    parser.add_argument("--root", default=".", help="Repository root")
    parser.add_argument(
        "--schema",
        default="App/config_schema.json",
        help="Path to JSON schema relative to root",
    )
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    schema_path = (root / args.schema).resolve()
    schema = json.loads(schema_path.read_text(encoding="utf-8"))

    used_keywords: set[str] = set()
    unknown_keywords: list[str] = []
    walk_schema(schema, "#", used=used_keywords, unknown=unknown_keywords)

    unsupported = sorted(used_keywords - SUPPORTED_VALIDATION_KEYWORDS)
    if unsupported:
        print("Unsupported validation keywords found:")
        for key in unsupported:
            print(f"  - {key}")
        raise SystemExit(1)

    if unknown_keywords:
        print("Unknown schema keywords encountered (review required):")
        for entry in unknown_keywords:
            print(f"  - {entry}")
        raise SystemExit(1)

    print(
        "schema keyword coverage check passed:",
        ", ".join(sorted(used_keywords)) if used_keywords else "no validation keywords detected",
    )


if __name__ == "__main__":
    main()
