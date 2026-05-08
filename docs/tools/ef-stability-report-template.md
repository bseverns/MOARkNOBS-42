# EF Stability Report Template

Hardware verification only. Software simulation can prepare inputs but cannot pass this report.

| Field                | Value           |
| -------------------- | --------------- |
| Date/time            | _fill_          |
| Tester               | _fill_          |
| Firmware SHA         | _fill_          |
| Audio source / level | _fill_          |
| Input channel(s)     | _fill_          |
| Board power profile  | POWER_CHOKED_V1 |

## Results

| Check                                          | Result            | Evidence / notes |
| ---------------------------------------------- | ----------------- | ---------------- |
| Idle floor suppresses disconnected float       | REQUIRES HARDWARE | _fill_           |
| Peak/RMS/gate modes react without stuck values | REQUIRES HARDWARE | _fill_           |
| Auto-baseline converges after silence          | REQUIRES HARDWARE | _fill_           |
| No audible/visible instability during movement | REQUIRES HARDWARE | _fill_           |
