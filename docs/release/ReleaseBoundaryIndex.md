# Release Boundary Index

> **Doc class:** Evidence index. This page separates current hardware-test status from beta/public release claims. It does not replace [Release Criteria](ReleaseCriteria.md).

MN42 release language should stay tied to evidence. A passing software suite, a working bench setup, or a generated artifact is useful, but none of those alone turns the repo into a beta or public release.

## Boundary Table

| Boundary      | Current Meaning                                                                           | Required Evidence Before Widening                                                                                                                                                                              |
| ------------- | ----------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Hardware-test | The repo can build, test, and document the current prototype/hardware-test package.       | Software checks, firmware build, conservative manifest defaults, and dated bench receipts for hardware behavior.                                                                                               |
| Demo          | A specific rig can be used for a rehearsal, classroom, or artist demo with known caveats. | Hardware-test evidence plus a dated demo/punch-list pass for the exact board, host, browser, Bridge path, and firmware tag.                                                                                    |
| Beta          | A small external group can use the package with support boundaries stated up front.       | Demo evidence plus signed bridge artifacts where required, host recipes with receipts, release verification summary, hardware validation reports, and documented recovery paths.                               |
| Public        | A broader audience can consume the release without insider setup knowledge.               | Beta evidence plus stable release artifacts, signed/notarized or otherwise platform-appropriate Bridge packaging, host compatibility evidence, fabrication/assembly artifacts, and resolved high-risk caveats. |

## Current Repo Claim

The current safe claim is **hardware-test package**.

Treat these as current boundaries unless a dated release note says otherwise:

- default firmware env: `teensy40_main`
- default power profile: `POWER_CHOKED_V1`
- rail topology verified: `false`
- Bridge CI artifacts: unsigned evidence/distribution bundles, not signed public installers
- direct browser path: strongest evidence is Chromium-based WebSerial
- desktop path: strongest evidence is Node 24 Bridge with console and `/app/`

## What Counts As Evidence

| Evidence Type              | Where It Lives                                          |
| -------------------------- | ------------------------------------------------------- |
| Test commands and lanes    | [TESTING](../validation/TESTING.md)                     |
| Go/no-go flow              | [Validation Flow](../validation/ValidationFlow.md)      |
| Required release artifacts | [Release Criteria](ReleaseCriteria.md)                  |
| Release checklist          | [Release Guide](ReleaseGuide.md)                        |
| Bridge signing status      | [Bridge Signing Plan](BridgeSigningPlan.md)             |
| Observed bench receipts    | [Bench Receipts](../bench/README.md)                    |
| Host support boundaries    | [Host Compatibility](../reference/HostCompatibility.md) |

## Language Rules

- Say **hardware-test** when the evidence is repo-local build/test/bench proof.
- Say **demo-ready for this rig** only when the exact rig has a dated pass.
- Say **beta** only when support, signing/packaging, recovery, and hardware evidence are ready for external users.
- Say **public release** only when the release path no longer depends on insider setup knowledge.
- Do not turn a single observed host recipe into a universal DAW/browser claim.

## Useful Commands

```bash
python3 tools/check_release_readiness.py --root . --stage hardware-test --env teensy40_main
python3 tools/doctor.py --release
```

Use `--stage beta` or `--stage public` only when you are deliberately testing those wider release claims.
