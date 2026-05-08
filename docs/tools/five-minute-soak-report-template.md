# Five-Minute Soak Report Template

Hardware verification only. Do not mark any item PASS without a real powered hardware run.

| Field                        | Value           |
| ---------------------------- | --------------- |
| Date/time                    | _fill_          |
| Tester                       | _fill_          |
| Firmware SHA                 | _fill_          |
| Board power profile          | POWER_CHOKED_V1 |
| LED brightness cap           | 26              |
| Rail topology verified       | no              |
| Supply voltage/current limit | _fill_          |
| Host/OS                      | _fill_          |

## Procedure

1. Boot `teensy40_main` with the intended hardware stack connected.
2. Send `HELLO` and confirm `{"hello":"mn42"}`.
3. Let the controller idle and then exercise normal controls for five minutes.
4. Record brownout, disconnect, thermal, OLED, LED, MIDI, and bridge symptoms.

## Results

| Check                                 | Result            | Evidence / notes |
| ------------------------------------- | ----------------- | ---------------- |
| No brownout/reset observed            | REQUIRES HARDWARE | _fill_           |
| USB connection remained stable        | REQUIRES HARDWARE | _fill_           |
| OLED remained responsive              | REQUIRES HARDWARE | _fill_           |
| LED brightness stayed at or below cap | REQUIRES HARDWARE | _fill_           |
| Controls remained responsive          | REQUIRES HARDWARE | _fill_           |
