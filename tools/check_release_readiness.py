#!/usr/bin/env python3
"""Report release-readiness blockers without pretending to validate hardware."""

from __future__ import annotations

import argparse
import configparser
import os
import pathlib
import re
import subprocess
import sys


WINDOWS_RESERVED_NAMES = {
    "CON",
    "PRN",
    "AUX",
    "NUL",
    *(f"COM{i}" for i in range(1, 10)),
    *(f"LPT{i}" for i in range(1, 10)),
}
WINDOWS_INVALID_CHARS = set('<>:"\\|?*')
POWER_PROFILES = ("POWER_CHOKED_V1", "SPLIT_RAIL_REWORK")
INTERPOLATION_RE = re.compile(r"\$\{([^}]+)\}")
BUILD_FLAG_PROFILE_RE = re.compile(
    r"(?<!\S)-DMN42_BOARD_POWER_PROFILE=(POWER_CHOKED_V1|SPLIT_RAIL_REWORK)\b"
)
REWORKED_VALIDATION_MARKER = "Reworked rail validation: PASS"


def has_any(root: pathlib.Path, patterns: tuple[str, ...]) -> bool:
    for pattern in patterns:
        if any(root.glob(pattern)):
            return True
    return False


def add_issue(
    *,
    stage: str,
    message: str,
    blockers: list[str],
    warnings: list[str],
    hard_stages: tuple[str, ...],
) -> None:
    if stage in hard_stages:
        blockers.append(message)
    else:
        warnings.append(message)


def parse_power_profiles(board_power_h: str) -> dict[str, dict[str, object]]:
    default_match = re.search(
        r"#define MN42_BOARD_POWER_PROFILE (POWER_CHOKED_V1|SPLIT_RAIL_REWORK)",
        board_power_h,
    )
    default_profile = default_match.group(1) if default_match else "UNKNOWN"
    profiles: dict[str, dict[str, object]] = {}
    for profile in POWER_PROFILES:
        block_match = re.search(
            rf"#(?:if|elif) MN42_BOARD_POWER_PROFILE == {re.escape(profile)}(?P<body>.*?)(?:#elif|#else|#endif)",
            board_power_h,
            re.DOTALL,
        )
        block = block_match.group("body") if block_match else ""
        profiles[profile] = {
            "profile": profile,
            "rail_verified": "MN42_BOARD_POWER_PROFILE_RAIL_TOPOLOGY_VERIFIED true" in block,
        }
    profiles["__default__"] = {"profile": default_profile}
    return profiles


def load_platformio_config(path: pathlib.Path) -> configparser.RawConfigParser:
    config = configparser.RawConfigParser(
        interpolation=None,
        delimiters=("="),
        inline_comment_prefixes=(";", "#"),
        strict=False,
    )
    config.optionxform = str
    config.read(path, encoding="utf-8")
    return config


def split_extends(value: str) -> list[str]:
    return [entry.strip() for entry in value.split(",") if entry.strip()]


def get_raw_option(
    config: configparser.RawConfigParser,
    section: str,
    option: str,
    seen: set[tuple[str, str]] | None = None,
) -> str:
    if seen is None:
        seen = set()
    key = (section, option)
    if key in seen:
        raise ValueError(f"cyclic platformio lookup for {section}.{option}")
    seen.add(key)

    if not config.has_section(section):
        raise KeyError(f"missing platformio section {section}")
    if config.has_option(section, option):
        return config.get(section, option, raw=True)
    if config.has_option(section, "extends"):
        for parent in split_extends(config.get(section, "extends", raw=True)):
            try:
                return get_raw_option(config, parent, option, seen.copy())
            except KeyError:
                continue
    raise KeyError(f"missing platformio option {section}.{option}")


def resolve_option(
    config: configparser.RawConfigParser,
    section: str,
    option: str,
    stack: set[tuple[str, str]] | None = None,
) -> str:
    if stack is None:
        stack = set()
    key = (section, option)
    if key in stack:
        raise ValueError(f"cyclic platformio interpolation for {section}.{option}")
    stack = stack | {key}

    raw = get_raw_option(config, section, option)

    def replace(match: re.Match[str]) -> str:
        reference = match.group(1)
        if "." not in reference:
            raise ValueError(f"unsupported platformio interpolation reference {reference!r}")
        ref_section, ref_option = reference.rsplit(".", 1)
        return resolve_option(config, ref_section, ref_option, stack)

    return INTERPOLATION_RE.sub(replace, raw)


def default_platformio_env(config: configparser.RawConfigParser) -> str:
    default_envs = config.get("platformio", "default_envs", raw=True, fallback="teensy40_main")
    for candidate in re.split(r"[\s,]+", default_envs.strip()):
        if candidate:
            return candidate
    return "teensy40_main"


def resolve_platformio_env_profile(
    config: configparser.RawConfigParser,
    env_name: str,
    profiles: dict[str, dict[str, object]],
) -> dict[str, object]:
    section = env_name if config.has_section(env_name) else f"env:{env_name}"
    build_flags = resolve_option(config, section, "build_flags")
    match = BUILD_FLAG_PROFILE_RE.search(build_flags)
    active_profile = match.group(1) if match else str(profiles["__default__"]["profile"])
    profile = dict(profiles.get(active_profile, {"profile": active_profile, "rail_verified": False}))
    profile["env"] = section.removeprefix("env:")
    return profile


def has_reworked_validation_evidence(root: pathlib.Path) -> bool:
    reports_dir = root / "docs" / "validation" / "reports"
    for path in reports_dir.glob("*.md"):
        text = path.read_text(encoding="utf-8", errors="ignore")
        if REWORKED_VALIDATION_MARKER in text and "SPLIT_RAIL_REWORK" in text:
            return True
    return False


def tracked_paths(root: pathlib.Path) -> list[str]:
    result = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=root,
        check=True,
        capture_output=True,
    )
    return [
        path.decode("utf-8", "surrogateescape")
        for path in result.stdout.split(b"\0")
        if path
    ]


def windows_path_issues(path: str) -> list[str]:
    issues: list[str] = []
    normalized = path.replace("/", os.sep)
    for segment in pathlib.PurePosixPath(path).parts:
        if segment in {".", ".."}:
            continue
        if segment.endswith(" ") or segment.endswith("."):
            issues.append(f"segment '{segment}' ends with a Windows-invalid trailing space/dot")
        if any(char in WINDOWS_INVALID_CHARS for char in segment):
            issues.append(f"segment '{segment}' contains a Windows-invalid character")
        stem = segment.split(".", 1)[0].upper()
        if stem in WINDOWS_RESERVED_NAMES:
            issues.append(f"segment '{segment}' uses reserved Windows device name '{stem}'")
    if "\\" in path:
        issues.append(f"path '{normalized}' contains backslashes in tracked name")
    return issues


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=".", help="Repository root")
    parser.add_argument(
        "--stage",
        choices=("hardware-test", "beta", "public"),
        default="hardware-test",
        help="Release gate to evaluate",
    )
    parser.add_argument(
        "--beta-scope",
        choices=("hardware", "full"),
        default="hardware",
        help="Whether beta readiness should require packaged Bridge artifacts.",
    )
    parser.add_argument(
        "--env",
        help="PlatformIO firmware environment to evaluate (defaults to firmware/platformio.ini default_envs).",
    )
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    blockers: list[str] = []
    warnings: list[str] = []

    bad_windows_paths: list[str] = []
    for tracked in tracked_paths(root):
        issues = windows_path_issues(tracked)
        for issue in issues:
            bad_windows_paths.append(f"{tracked}: {issue}")
    if bad_windows_paths:
        for issue in bad_windows_paths:
            blockers.append(f"windows checkout hazard: {issue}")

    board_power_h = (root / "firmware/include/BoardPowerProfile.h").read_text(encoding="utf-8")
    power_profiles = parse_power_profiles(board_power_h)
    platformio_config = load_platformio_config(root / "firmware" / "platformio.ini")
    env_name = args.env or default_platformio_env(platformio_config)
    power = resolve_platformio_env_profile(platformio_config, env_name, power_profiles)

    gerber_present = has_any(root, ("hardware/**/*[Gg]erber*.zip", "hardware/**/*[Gg]erber*/**/*"))
    drill_present = has_any(root, ("hardware/**/*[Dd]rill*.zip", "hardware/**/*[Dd]rill*/**/*"))
    bom_present = has_any(root, ("hardware/**/*[Bb][Oo][Mm]*", "docs/**/*[Bb][Oo][Mm]*"))
    pnp_present = has_any(root, ("hardware/**/*[Pp]ick*[Pp]lace*", "hardware/**/*[Pp][Nn][Pp]*"))
    assembly_present = has_any(root, ("hardware/**/*[Aa]ssembly*", "docs/**/*[Aa]ssembly*"))

    if not gerber_present or not drill_present:
        add_issue(
            stage=args.stage,
            message="missing or unverified Gerber/NC-drill fabrication bundle",
            blockers=blockers,
            warnings=warnings,
            hard_stages=("beta", "public"),
        )
    if not bom_present or not pnp_present or not assembly_present:
        add_issue(
            stage=args.stage,
            message="missing BOM, pick-and-place, or assembly notes",
            blockers=blockers,
            warnings=warnings,
            hard_stages=("beta", "public"),
        )
    if not power["rail_verified"]:
        add_issue(
            stage=args.stage,
            message=f"rail topology is not verified for env {power['env']} (profile {power['profile']})",
            blockers=blockers,
            warnings=warnings,
            hard_stages=("beta", "public"),
        )
    if power["profile"] == "SPLIT_RAIL_REWORK" and not has_reworked_validation_evidence(root):
        add_issue(
            stage=args.stage,
            message=(
                f"env {power['env']} uses SPLIT_RAIL_REWORK without a dated validation receipt; "
                "add a docs/validation/reports/*.md report containing "
                f"'{REWORKED_VALIDATION_MARKER}' before beta/public release"
            ),
            blockers=blockers,
            warnings=warnings,
            hard_stages=("beta", "public"),
        )

    installer_present = has_any(root, ("bridge/dist/*.exe", "bridge/dist/*.pkg", "bridge/dist/*.dmg"))
    signature_present = has_any(root, ("bridge/dist/*.sig", "bridge/dist/*.asc"))
    if not installer_present or not signature_present:
        if args.stage == "public" or (args.stage == "beta" and args.beta_scope == "full"):
            blockers.append("unsigned bridge installer for beta/public stage")
        elif args.stage == "beta":
            warnings.append("beta hardware scope: signed bridge installer not required")
        else:
            warnings.append("hardware-test stage: signed bridge installer is not required")

    report_present = has_any(root, ("docs/validation/reports/*.md",))
    if not report_present:
        add_issue(
            stage=args.stage,
            message="no dated hardware validation reports found under docs/validation/reports/",
            blockers=blockers,
            warnings=warnings,
            hard_stages=("beta", "public"),
        )

    for warning in warnings:
        print(f"warning: {warning}")
    if blockers:
        for blocker in blockers:
            print(f"blocker: {blocker}")
        print(f"release readiness failed: {len(blockers)} blocker(s)", file=sys.stderr)
        raise SystemExit(1)

    if args.stage == "hardware-test" and warnings:
        print("release readiness: not release-ready, but acceptable for hardware-test package")
        return

    print(f"release readiness passed for stage {args.stage}")


if __name__ == "__main__":
    main()
