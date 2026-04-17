Import("env")

# Match the firmware helper so repo-root PlatformIO shim builds keep the same
# warning profile and build-dir bootstrap behavior.
env.Append(CXXFLAGS=["-Wno-deprecated-copy"])

from pathlib import Path

Path(env.subst("$BUILD_DIR")).mkdir(parents=True, exist_ok=True)
