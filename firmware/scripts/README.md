# Firmware Build Scripts

This is the broom closet where we stash little Python gremlins that tweak PlatformIO before the compiler spins up.

## `deprecated_copy_flag.py`

PlatformIO slurps in this script via `extra_scripts = pre:scripts/deprecated_copy_flag.py` in `firmware/platformio.ini`.
The repo-root `platformio.ini` is a shim that can still trigger extra-script resolution from repo root, so `scripts/deprecated_copy_flag.py` exists there as a tiny forwarder to this canonical helper. Fire `pio` from inside `firmware/` (or use `pio <command> -d firmware …` from anywhere) and you'll still land on this file for the real behavior. When the hook runs it simply calls:

```python
env.Append(CXXFLAGS=["-Wno-deprecated-copy"])
```

That one-liner bolts the `-Wno-deprecated-copy` flag onto every C++ compile, keeping noisy warnings from drowning out real problems.

## When to mess with this folder

Only dive in here when you need to rig the build system—adding or yanking compiler flags, or other pre-build voodoo. Regular firmware code lives elsewhere.

## Adding more pre-build hooks

1. Drop a Python script in this directory.
2. Wire it up in `platformio.ini` under `extra_scripts` with the `pre:` prefix:

```ini
extra_scripts =
    pre:scripts/my_new_hotness.py
```

Keep these scripts small, loud, and well‑commented so the next punk knows why they exist.

## `version.py`

This troublemaker prints out two `-D` flags every time the build spins up:
`FW_VERSION` comes from the environment and `GIT_SHA` tries to grab the short
commit hash. If Git ghosts us—maybe you're building from a source zip or the repo
isn't around—it just stamps `GIT_SHA=unknown` and keeps the party going.

## `require_unity_test_target.py`

Loaded only by `[env:teensy40_unity]`. This guard hard-fails if someone runs
that environment through `pio run` instead of `pio test`.

Why: `teensy40_unity` is assembled for the custom Unity harness and test entry
points. Running it as a plain build creates noisy, misleading failures that
look like firmware breakage but are really invocation mistakes.

Accepted commands:

```bash
pio test -d firmware -e teensy40_unity -vvv
# or from firmware/
pio test -e teensy40_unity -vvv
```
