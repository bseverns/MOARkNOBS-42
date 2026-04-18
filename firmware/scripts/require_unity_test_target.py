"""Guardrails for the Unity test environment.

`[env:teensy40_unity]` is intentionally test-only. Building it via `pio run`
causes confusing link failures because Unity entry points and test harness
objects are assembled for `pio test`.
"""

Import("env")
from SCons.Script import COMMAND_LINE_TARGETS


targets = [str(target).lower() for target in (COMMAND_LINE_TARGETS or [])]
non_test_targets = {"clean", "envdump", "idedata", "compiledb"}

# Allow metadata / cleanup targets to run without forcing the full test flow.
if not targets or not set(targets).issubset(non_test_targets):
    if not any("test" in target for target in targets):
        print(
            "\nERROR: [env:teensy40_unity] is test-only.\n"
            "Use:\n"
            "  pio -d firmware test -e teensy40_unity -vvv\n"
            "or from firmware/:\n"
            "  pio test -e teensy40_unity -vvv\n"
        )
        env.Exit(1)
