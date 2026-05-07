Import("env")

# Slip -Wno-deprecated-copy in only for C++ builds.
env.Append(CXXFLAGS=["-Wno-deprecated-copy"])

# Teensy's USB string override symbols use flexible-array descriptors. With LTO,
# GCC compares our concrete replacement size against the core's incomplete extern.
env.Append(LINKFLAGS=["-Wno-lto-type-mismatch"])

# PlatformIO normally spins up the variant build directory (`.pio/build/<env>`)
# before invoking any builders, but some VS Code tasks were tripping over a race
# where SCons tried to open `.sconsign311.dblite` before the folder existed.
# Make sure the directory tree is around so the teensy40_unity tests don't faceplant.
from pathlib import Path

Path(env.subst("$BUILD_DIR")).mkdir(parents=True, exist_ok=True)
