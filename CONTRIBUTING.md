# Contributing to MOARkNOBS-42

So you want to lob code grenades into this synth monster? Rad. Keep it loud but keep it tight.
For the full rulebook see [AGENTS.md](AGENTS.md), but here's the cheat sheet.

## Build & Flash

The PlatformIO project lives under `firmware/`—the repo root is intentionally blocked for `pio` commands.

```bash
# Main firmware
pio run -d firmware -e teensy40_main
```

Other build targets you can flex with:

- `teensy40_full_system`
- `teensy40_unified_test`
- `teensy40_biquad_test`
- `teensy40_eeprom_persistence`
- `teensy40_slot_verify`

## Run the Gauntlet (Tests)

From the repo root, crank up the Unity tests:

```bash
pio test -d firmware -e teensy40_unity -vvv
```

Unity screams over a custom `Serial1` transport—do **not** lean on the default Serial or regenerate PlatformIO's unittest transport.

Run docs and contract guards before pushing release/docs changes:

```bash
python3 tools/check_markdown_links.py --root .
python3 tools/check_wiki_contract.py --root .
python3 tools/check_schema_keyword_coverage.py --root .
python3 tools/check_comment_style.py --root .
python3 tools/check_contract_sync.py --root .
python3 tools/check_control_coverage.py --root .
python3 tools/check_release_readiness.py --root . --stage hardware-test
```

Or run the grouped guardrails:

```bash
python3 tools/doctor.py --docs
python3 tools/doctor.py --app
python3 tools/doctor.py --bridge
python3 tools/doctor.py --release
python3 tools/doctor.py --full
```

Use the heavier build/test lanes when the change touches runtime behavior:

```bash
npm --prefix App test
npm --prefix bridge test
pio run -d firmware -e teensy40_main
pio test -d firmware -e teensy40_unity -vvv
```

## Code Etiquette

- Globals go `extern` in headers and live in exactly one `.cpp`.
- Comments are contracts. Break one? Update it. Follow the [comment style](docs/reference/CommentStyle.md) for new comments. Touch a public API? Document it in `README.md`.
- Keep the PlatformIO stuff in `./firmware`; don't treat the root like a project.
- Tests run in USB*MIDI_SERIAL mode only. Don't sneak in other USB*\* defines.
- The unit-test environment is lean: no Adafruit GFX/SSD1306/BusIO or SD/SdFat unless absolutely required.

## Accessibility Notes

- Every image in docs needs alt text; "pic here" won't cut it.
- Don't use color alone to signal meaning—pair it with words or symbols.
- Headings should step down one level at a time so screen readers don't get whiplash.

## Before You Shred

Unsure about a move? Ask first. We dig bold ideas, not reckless chaos.

For every gnarly detail, read [AGENTS.md](AGENTS.md) and commit like you mean it.
