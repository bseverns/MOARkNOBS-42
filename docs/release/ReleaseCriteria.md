# Release Criteria

This repository is currently a hardware-test package. Public or beta release status requires explicit evidence, not inference from passing software tests.

## Required Artifacts

| Area             | Required evidence                                                              | Software-checkable               |
| ---------------- | ------------------------------------------------------------------------------ | -------------------------------- |
| Fabrication      | Gerber ZIP and NC-drill bundle for the intended board revision                 | artifact presence only           |
| Assembly         | BOM, pick-and-place data, and assembly notes                                   | artifact presence only           |
| Power topology   | `rail_topology_verified=true` only after documented hardware validation        | manifest/docs consistency only   |
| Bridge packaging | signed bridge installer for public/beta stages                                 | signature/artifact presence only |
| Validation       | dated reports for soak, EF stability, EXT clock starvation, and panic baseline | report presence only             |

## Current Boundary

- Active power profile: `POWER_CHOKED_V1`
- LED brightness cap: `26`
- Rail topology verified: `false`
- `SPLIT_RAIL_REWORK` is not release-safe until the topology is deliberately verified and documented.

Run:

```bash
python3 tools/check_release_readiness.py --root . --stage hardware-test
```

Use `--stage beta` or `--stage public` only when preparing an actual release gate. The tool reports blockers; it does not verify electrical correctness.
