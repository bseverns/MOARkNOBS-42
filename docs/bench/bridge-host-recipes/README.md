# Bridge Host Recipe Receipts

This folder holds host-specific Bridge recipe receipts.

This is an evidence folder. For tie-break rules, see [Documentation Truth Map](../../reference/DocumentationTruthMap.md).

## What belongs here

- dated host recipe receipts tied to a specific DAW/app, host OS, Bridge mode, and routing setup
- observation receipts when a host/app was present but the full recipe was not completed
- links to supporting Bridge HIL receipts, screenshots, or logs captured during the host run

## Evidence rule

- A documented recipe is not the same thing as a verified host path.
- Treat a host/app combination as evidence-backed only when a matching receipt exists.
- Treat only `PASS` receipts as verified evidence for that exact host/app/version/setup combination.
- `PARTIAL` or `FAIL` receipts are still useful, but they do not widen support claims.

## Current references

- [Host recipe receipt template](TEMPLATE_host-recipe-receipt.md)
- [2026-05-31 macOS IAC + REAPER observation receipt](macos-iac-reaper-basic-observed.md)

## Receipt rules

- Capture the exact host/app/version observed.
- Record whether the Bridge was `source` or `packaged`.
- Record the exact OSC ports, MIDI port label, and serial device or simulator lane.
- List the exact actions tested, not just the intended recipe.
- If DAW-side capture or return traffic was not directly observed, say that plainly.
- Do not claim a recipe is verified just because the Bridge reached `ready`.
