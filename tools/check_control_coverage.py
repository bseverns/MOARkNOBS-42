#!/usr/bin/env python3
"""Check on-device combo docs against the firmware combo registry."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys


CONTROL_RE = re.compile(
    r"(?:(long-press|hold)\s+)?`?(Ctrl\d(?:\s*\+\s*(?:Ctrl)?\d)+|Ctrl\d)`?",
    re.IGNORECASE,
)


def normalize_control(text: str) -> str:
    text = text.strip().replace(" ", "")
    text = text.replace("`", "")
    lower = text.lower()
    prefix = ""
    if lower.startswith("long-press"):
        prefix = "long-press "
        text = text[len("long-press") :]
    elif lower.startswith("hold"):
        prefix = "hold "
        text = text[len("hold") :]
    numbers = re.findall(r"(?:Ctrl)?(\d)", text, flags=re.IGNORECASE)
    return prefix + "+".join(f"Ctrl{number}" for number in numbers)


def extract_controls(text: str) -> set[str]:
    controls: set[str] = set()
    for match in CONTROL_RE.finditer(text):
        prefix = f"{match.group(1).lower()} " if match.group(1) else ""
        controls.add(normalize_control(prefix + match.group(2)))
    return controls


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=".", help="Repository root")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    registry_path = root / "docs/reference/on_device_control_registry.json"
    coverage_path = root / "docs/validation/OnDeviceControlCoverage.md"
    guide_path = root / "docs/guides/ComboGuide.md"
    implementation_paths = (
        root / "firmware/include/ButtonManager.h",
        root / "firmware/src/ButtonManager.cpp",
    )

    registry = json.loads(registry_path.read_text(encoding="utf-8"))
    if registry.get("schema_version") != 2:
        raise SystemExit("control registry must use schema_version 2")

    namespace_buttons = [item.get("button") for item in registry.get("namespaces", [])]
    if sorted(namespace_buttons) != list(range(6)):
        raise SystemExit("control registry namespaces must define Ctrl0 through Ctrl5 exactly once")

    context_masks: set[int] = set()
    for context in registry.get("contexts", []):
        buttons = context.get("mask", [])
        if not buttons or any(not isinstance(button, int) or button < 0 or button > 5 for button in buttons):
            raise SystemExit(f"invalid contextual help mask: {buttons!r}")
        mask = sum(1 << button for button in set(buttons))
        if mask in context_masks:
            raise SystemExit(f"duplicate contextual help mask: {buttons!r}")
        context_masks.add(mask)
        for key in ("line1", "line2", "line3"):
            if not context.get(key) or len(context[key]) > 21:
                raise SystemExit(f"invalid OLED help {key} for mask {buttons!r}")

    if not all((1 << button) in context_masks for button in range(6)):
        raise SystemExit("contextual help must define a root view for Ctrl0 through Ctrl5")

    required_combo_fields = {"id", "control", "label", "layer", "family", "reversible"}
    for item in registry.get("combos", []):
        missing = required_combo_fields - item.keys()
        if missing:
            raise SystemExit(f"{item.get('id', '<unknown>')}: missing registry fields {sorted(missing)}")
        if item["layer"] not in {"immediate", "modifier", "edit", "browser"}:
            raise SystemExit(f"{item['id']}: invalid control layer {item['layer']!r}")
        if "+" in item["control"] and (not item.get("oled") or len(item["oled"]) > 16):
            raise SystemExit(f"{item['id']}: chord requires an OLED label of at most 16 characters")

    expected = {normalize_control(item["control"]): item["id"] for item in registry["combos"]}
    coverage_controls = extract_controls(coverage_path.read_text(encoding="utf-8"))
    guide_controls = extract_controls(guide_path.read_text(encoding="utf-8"))
    implementation_text = "\n".join(path.read_text(encoding="utf-8") for path in implementation_paths)
    implementation_controls = extract_controls(implementation_text)

    errors: list[str] = []
    for control, item_id in sorted(expected.items()):
        base_control = control.replace("long-press ", "").replace("hold ", "")
        if control not in implementation_controls and base_control not in implementation_controls:
            errors.append(f"{item_id}: {control} missing from ButtonManager.h combo map")
        if control not in coverage_controls and control not in guide_controls:
            errors.append(f"{item_id}: {control} missing from validation docs/ComboGuide")

    expected_bases = {
        control.replace("long-press ", "").replace("hold ", "") for control in expected
    }
    undocumented = sorted(
        control
        for control in implementation_controls
        if "+" in control and control not in expected and control not in expected_bases
    )
    if undocumented:
        errors.append(
            "implemented combo(s) missing from registry: " + ", ".join(undocumented)
        )

    if errors:
        for error in errors:
            print(error)
        raise SystemExit(1)

    subprocess.run(
        [sys.executable, str(root / "tools/generate_control_artifacts.py"), "--root", str(root), "--check"],
        check=True,
    )

    print(f"control coverage check passed ({len(expected)} registered controls)")


if __name__ == "__main__":
    main()
