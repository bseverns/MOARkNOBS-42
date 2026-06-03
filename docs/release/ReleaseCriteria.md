# Release Criteria

> **Doc class:** Evidence doc. This page defines release gates and required artifacts; it does not claim release readiness by itself.

This repository is currently a hardware-test package. Public or beta release status requires explicit evidence, not inference from passing software tests.

For a reader-facing split between hardware-test, demo, beta, and public claims, start with [Release Boundary Index](ReleaseBoundaryIndex.md).

## Required Artifacts

| Area             | Required evidence                                                              | Software-checkable               |
| ---------------- | ------------------------------------------------------------------------------ | -------------------------------- |
| Fabrication      | Gerber ZIP and NC-drill bundle for the intended board revision                 | artifact presence only           |
| Assembly         | BOM, pick-and-place data, and assembly notes                                   | artifact presence only           |
| Power topology   | `rail_topology_verified=true` only after documented hardware validation        | manifest/docs consistency only   |
| Bridge packaging | signed bridge installer for public/beta stages                                 | signature/artifact presence only |
| Validation       | dated reports for soak, EF stability, EXT clock starvation, and panic baseline | report presence only             |

## Current Boundary

- Default firmware env: `teensy40_main`
- Active power profile for that default env: `POWER_CHOKED_V1`
- LED brightness cap: `26`
- Rail topology verified: `false`
- Reworked-only env: `teensy40_main_reworked`
- `SPLIT_RAIL_REWORK` is not release-safe until the topology is deliberately verified and documented in a dated receipt.
- Host tools may warn when the manifest reports `SPLIT_RAIL_REWORK` or `rail_topology_verified=true` outside that documented boundary, but the warning is not evidence by itself.
- Beta/public use of `teensy40_main_reworked` stays blocked unless a report under `docs/validation/reports/` explicitly includes `Reworked rail validation: PASS`.

Run:

```bash
python3 tools/check_release_readiness.py --root . --stage hardware-test --env teensy40_main
```

Use `--stage beta` or `--stage public` only when preparing an actual release gate. Pass `--env teensy40_main_reworked` only when the physical board actually matches that reworked topology. The tool reports blockers; it does not verify electrical correctness.
