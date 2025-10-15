# VS Code IntelliSense cheat sheet (a little noise, a lot of signal)

Welcome to the scrappy side of the studio notebook. If VS Code is screaming about missing headers like `FastLED.h` or `MIDI.h`, it's not your code—it's IntelliSense reading stale, machine-specific paths. Here's how we tame it.

## What's going on?

PlatformIO used to auto-spit a `c_cpp_properties.json` loaded with absolute paths from the original developer's machine. New contributors inherit that fossil and IntelliSense faceplants before we even plug in the Teensy.

## The fix baked into this repo

We now hand-roll the VS Code C/C++ config so it:

- Points to repo-relative include folders (src/, include/, lib/, tests) instead of `/Users/someone/...` ghosts.
- Leans on the PlatformIO extension as the configuration provider so your local toolchain fills in any extra paths.
- Falls back to the typical `${HOME}/.platformio` locations if PlatformIO hasn't been initialized yet.

That combo keeps IntelliSense aware of vendored FastLED + friends and the MIDI stubs we ship for tests.

## How to refresh IntelliSense after cloning

1. Make sure you've opened the repo root in VS Code (not the firmware/ subfolder).
2. Run the PlatformIO "Rebuild IntelliSense Index" command (hit <kbd>Cmd/Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>P</kbd> and search for it) after installing dependencies.
3. If you haven't built yet, run `pio run` once from `firmware/` so PlatformIO populates `.pio/libdeps/` with the external libs we reference.

Do those steps and the red squiggles around `FastLED.h` / `MIDI.h` disappear. No more psychic debugging of someone else's filesystem.

## Want to tweak further?

- Need a one-off include for a prototype? Add it to `.vscode/c_cpp_properties.json` locally, but keep our checked-in version as-is so the team stays in sync.
- Prefer compile-commands-based IntelliSense? You can point the C/C++ extension at `.pio/build/<env>/compile_commands.json` once you've built an environment.

Keep it loud, keep it readable, and keep the tools out of the way of the art.
