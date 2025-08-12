# TESTING.md

So you want to know if the knobs behave before you melt anything? Here's how the CI circus rolls.

## Unity tests

These are the fast, software-only checks that keep our logic tidy.

- **Trigger:** every push and pull request. Manual dispatch works too.
- **Command:** `platformio test -e teensy40_unity` from the `firmware/` directory.
- **Artifacts:** look for the `unity-test-logs` artifact. It's the receipt proving this job ran.

## System tests

These go after the hardware. They only run when you mean it.

- **Trigger:** push to the `hardware-test` branch *or* smash the manual dispatch button in GitHub.
- **Command:** `platformio test -e teensy40_full_system` from `firmware/`.
- **Artifacts:** `system-test-logs` show up when this job actually fires.

## DIY

Run either suite locally with the same `platformio test` commands above. If you're not in `firmware/`, the tests will give you side-eye.

Stay loud, test often.
