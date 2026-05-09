#!/usr/bin/env python3
"""Report release-readiness blockers without pretending to validate hardware."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys


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


def parse_power_profile(board_power_h: str) -> dict[str, object]:
    active_match = re.search(r"#define MN42_BOARD_POWER_PROFILE (POWER_CHOKED_V1|SPLIT_RAIL_REWORK)", board_power_h)
    active = active_match.group(1) if active_match else "UNKNOWN"
    block_match = re.search(
        rf"#(?:if|elif) MN42_BOARD_POWER_PROFILE == {re.escape(active)}(?P<body>.*?)(?:#elif|#else|#endif)",
        board_power_h,
        re.DOTALL,
    )
    block = block_match.group("body") if block_match else ""
    return {
        "profile": active,
        "rail_verified": "MN42_BOARD_POWER_PROFILE_RAIL_TOPOLOGY_VERIFIED true" in block,
    }


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
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    blockers: list[str] = []
    warnings: list[str] = []

    board_power_h = (root / "firmware/include/BoardPowerProfile.h").read_text(encoding="utf-8")
    power = parse_power_profile(board_power_h)

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
            message=f"rail topology is not verified for active profile {power['profile']}",
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
