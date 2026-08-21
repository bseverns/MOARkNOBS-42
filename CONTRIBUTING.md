# Contributing to MOARkNOBS-42

MN42 welcomes experiments, but contribution claims must match the evidence. A change is ready for review when its
affected build and test lanes pass, public behavior is documented, contracts remain synchronized, and hardware claims
are backed by a reproducible receipt.

The complete repository rules are in [AGENTS.md](AGENTS.md). This page is the contributor-facing checklist.

## Contribution checklist

Before opening a pull request:

- [ ] Keep the PlatformIO project rooted at `firmware/`; run firmware commands with `-d firmware` from repo root.
- [ ] Build every firmware environment affected by the change.
- [ ] Run the custom-transport Unity suite for firmware behavior changes.
- [ ] Run App or Bridge tests when their code, schemas, protocols, or documentation changes.
- [ ] Run contract, control-coverage, release-readiness, and documentation guards.
- [ ] Update public API and behavior documentation with the code change.
- [ ] Add alt text to images, keep headings sequential, and do not communicate state by color alone.
- [ ] Distinguish simulator, CI, and bench evidence; do not turn one into a broader hardware or host claim.

The one-command local readiness gate is:

```bash
python3 tools/doctor.py --full
```

If it cannot run in your environment, report which scoped commands you ran and why the remainder was skipped.

## Build and test

Build the main firmware:

```bash
pio run -d firmware -e teensy40_main
```

Other full-machine build environments include:

- `teensy40_full_system`
- `teensy40_unified_test`
- `teensy40_biquad_test`
- `teensy40_eeprom_persistence`
- `teensy40_slot_verify`

Run firmware software tests:

```bash
pio test -d firmware -e teensy40_unity -vvv
```

Unity output uses the custom functions in `firmware/test/unity_output.cpp` over `Serial1`. Do not regenerate
PlatformIO's Serial transport, add `-DSerial=…`, include `unittest_transport.h`, or change the test USB mode away from
`USB_MIDI_SERIAL`. Keep display and SD libraries out of the unit-test environment unless the change explicitly needs
them.

Run host tests when those surfaces are affected:

```bash
npm --prefix App test
npm --prefix bridge test
```

## Scoped guardrails

Use the smallest grouped check that covers the change while developing:

```bash
python3 tools/doctor.py --docs
python3 tools/doctor.py --app
python3 tools/doctor.py --bridge
python3 tools/doctor.py --release
python3 tools/doctor.py --firmware
```

The release-facing contract checks include:

```bash
python3 tools/check_contract_sync.py --root .
python3 tools/check_control_coverage.py --root .
python3 tools/check_release_readiness.py --root . --stage hardware-test
```

## Code and documentation standards

- Declare global variables as `extern` in headers and define them exactly once in a corresponding `.cpp` file.
- Treat comments as maintained contracts. Follow the [comment style](docs/reference/CommentStyle.md).
- Document public API or operator-visible behavior changes in the relevant README or contract page.
- Preserve the [Documentation Truth Map](docs/reference/DocumentationTruthMap.md): plans, evidence, and contracts are
  different kinds of truth.
- Keep the weird edge cases and the project voice, but make commands, safety boundaries, and expected results literal.

## Pull request notes

Summarize what changed, the user-visible effect, the checks you ran, and any unverified hardware or host boundary. Link
dated receipts for physical-board claims. If a change intentionally leaves a guard or test unrun, say so directly.

Unsure whether a change crosses a hardware, persistence, protocol, or release boundary? Ask before widening the scope.
Bold ideas are welcome; silent contract drift is not.
