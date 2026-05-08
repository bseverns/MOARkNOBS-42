# EXT Clock Starvation Report Template

Hardware verification only. Use this to document real external-clock behavior.

| Field             | Value  |
| ----------------- | ------ |
| Date/time         | _fill_ |
| Tester            | _fill_ |
| Firmware SHA      | _fill_ |
| Clock source      | _fill_ |
| Clock rate / PPQN | _fill_ |

## Results

| Check                                                                 | Result            | Evidence / notes |
| --------------------------------------------------------------------- | ----------------- | ---------------- |
| EXT clock is followed when present                                    | REQUIRES HARDWARE | _fill_           |
| Starvation falls back or holds according to current firmware behavior | REQUIRES HARDWARE | _fill_           |
| No MIDI flood or lockup during starvation                             | REQUIRES HARDWARE | _fill_           |
| Clock source OLED/status remains truthful                             | REQUIRES HARDWARE | _fill_           |
