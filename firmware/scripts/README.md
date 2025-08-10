# Firmware Build Scripts

This is the broom closet where we stash little Python gremlins that tweak PlatformIO before the compiler spins up.

## `deprecated_copy_flag.py`

PlatformIO slurps in this script via `extra_scripts = pre:scripts/deprecated_copy_flag.py` in `platformio.ini`. Once loaded, it calls:

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
