# Comment Style

Comments should help the next builder understand signal flow, safety rails,
protocol compatibility, timing budgets, hardware assumptions, and learner
landmarks. Do not comment obvious syntax.

## Default Shape

Use `//` for any comment that is one logical note, even when the line gets a
little long.

```cpp
// Clamp EEPROM-loaded ARG method values before they can index past the enum.
```

```js
// Views call the runtime shell, then listen for events instead of poking transport state directly.
```

Use block comments only for true multi-line teaching notes.

```cpp
/*
This manager scans the button matrix, debounces each switch, then turns stable
presses into short, double, long, and combo actions.

Keep this comment high-level. The branches below explain the unusual cases.
*/
```

Block comment rules:

- Put the opening `/*` on its own line.
- Put the closing `*/` on its own line.
- Do not use a leading `*` ladder.
- Do not use `/** ... */` unless Doxygen or JSDoc generation is intentionally
  adopted.
- Do not use inline `/* no-op */` comments except inside function signatures to
  name unused parameters.

Prefer this:

```cpp
// no-op
```

## Headers And Ledgers

Keep learner-orientation section headers in includes and other teaching-heavy
files. These ledgers give readers a map before they hit the machinery.

```cpp
// MODULE LEDGER - Human-facing map
// ================================
// Short explanation of what this file owns.
// Important modes, combos, or hardware assumptions.
// Where to look next.
```

Lower-level section dividers can stay simple:

```cpp
// ---- Multiplexer scan path ----
```

## Public APIs

Use plain block comments for classes, structs, and larger public APIs when the
comment has teaching value. Do not default to Doxygen-style comments unless the
repo starts generating Doxygen output.

```cpp
/*
Reads all potentiometers through the mux pair and forwards changed values through
the MIDI callback lane.
*/
class PotentiometerManager {
```

For tiny methods or fields, use `//`.

```cpp
// Link the manager with ConfigManager so channels and CCs stay in sync.
void attachConfigManager(ConfigManager &cfg);
```

## Inline Comments

Inline comments should explain why a branch exists, not what the language syntax
does. Good targets include:

- Hardware assumptions
- Units and ranges
- Timing budgets
- Protocol compatibility
- Weird safety behavior
- Reasons for non-obvious branches

```cpp
const uint8_t maxMethod = static_cast<uint8_t>(ARGMethod::XORR);
// EEPROM and JSON can hand us raw integers, so clamp before enum dispatch.
```

Avoid comments like `i++; // increment i`.

## Density By Area

Firmware `.cpp` files can carry the most learner context: top-of-file
orientation, comments above non-obvious branches, and block comments before
major subsystem flows.

Firmware `.h` files should use section headers or ledgers for reader maps,
plain block comments for high-value class and struct context, and `//` for
individual fields and methods.

App and bridge JavaScript should use `//` for function-level intent. Reserve
block comments for state-machine and protocol explanations. Avoid DOM narration.

Tests should stay sparse. Comment fixtures, fake transports, race timing, and
hardware shims when the reason is not obvious from the test name.

Python keeps module and function docstrings. Shell keeps `#`. YAML and Markdown
use their native forms. Tooling comments such as `/* eslint-env browser */`
should stay in the required tool format.

## Searchable Tags

Use all-caps tags only when they create a searchable maintenance signal.

```cpp
// TODO: add hardware-backed calibration evidence before enabling this path by default.
// FIXME: this assumes slot count is fixed at 42.
// NOTE: this branch preserves the legacy WebSerial payload shape.
// WARN: do not call this before EEPROM is mounted.
```

Allowed tags:

- `TODO`
- `FIXME`
- `NOTE`
- `WARN`
- `HACK`
- `DEPRECATED`

Use them sparingly.

## Migration

Adopt this convention for new comments immediately. When touching a file,
normalize nearby comments as part of the same change.

Do focused passes only when the scope is clear:

- Header pass: keep learner ledgers, convert Doxygen-style comments to plain
  block comments where Doxygen adds no value, and convert tiny block comments to
  `//`.
- Implementation pass: remove inline `/* no-op */`, and make major flow comments
  either single-note `//` comments or clean teaching blocks.

Short rule: `//` for one note. `/* ... */` for multi-line teaching blocks. Keep
include and header ledgers. No decorative star ladders.
