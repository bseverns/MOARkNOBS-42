# OLED Dominance Checklist

This checklist validates that temporary overlays never hide modal editing state.

## Scope

Display ownership should resolve in this order:

1. startup animation
2. modal edit views (`on-device config`, `LFO tune`, `jitter tune`, `diagnostics`)
3. held-control contextual command palette
4. status overlays (when no modal view is active)
5. control overlays (`filter`, `arp`, `arp edit`, `note dynamics`)
6. screensaver
7. baseline context view

## Test Cases

| ID    | Scenario                           | Trigger                                                 | Expected OLED Owner | Expected Result                                                            |
| ----- | ---------------------------------- | ------------------------------------------------------- | ------------------- | -------------------------------------------------------------------------- |
| OD-01 | Startup dominates                  | power cycle                                             | startup animation   | no status/context screen shown until startup finishes                      |
| OD-02 | Config mode dominates status       | enter config mode, then press controls that emit status | config mode view    | `[CONFIG] ...` view remains visible; brief status does not steal ownership |
| OD-03 | LFO mode dominates status          | enter LFO tune, move pots/buttons                       | LFO tune view       | LFO view remains persistent while edits occur                              |
| OD-04 | Jitter mode dominates status       | hold `Ctrl0+Ctrl3+Ctrl4`, move pots                     | jitter tune view    | jitter view stays visible while tuning and shows base/effective values     |
| OD-05 | Diagnostics dominates status       | enter diagnostics, trigger other events                 | diagnostics view    | diagnostic page remains active until exited                                |
| OD-06 | Status overlay dominates non-modal | outside modal modes, trigger status message             | status overlay      | status text is shown for timeout period                                    |
| OD-07 | Control overlay follows status     | outside modal modes, adjust arp/filter/note dynamics    | control overlay     | control overlay appears after status timeout expires                       |
| OD-08 | Screensaver only when idle         | no interaction >90s                                     | screensaver         | random-pixel screen appears only when no higher-priority owner is active   |
| OD-09 | Root chord help                     | hold each of `Ctrl0` through `Ctrl5`                     | command palette     | namespace and useful continuations remain visible while held               |
| OD-10 | Chord continuation help             | hold `Ctrl0+Ctrl1`                                      | command palette     | ARG action plus LFO Tune and LFO Live continuations are visible            |
| OD-11 | Modal view beats chord help         | hold a control while config/LFO/diagnostics is active   | modal view          | persistent bracketed mode header remains visible                           |
| OD-12 | Diagnostics reject jitter ownership | in diagnostics, hold `Ctrl0+Ctrl3+Ctrl4`                | diagnostics         | diagnostic OLED and white LED remain; jitter tuning does not activate      |

## Run Procedure

1. Flash and boot `teensy40_main`.
2. Execute OD-01 through OD-12 in order.
3. Mark each case `PASS` / `FAIL` with notes.

## Result Log

| Date | Firmware | Tester | OD-01..08 | OD-09 | OD-10 | OD-11 | OD-12 | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| _fill_ | _fill_ | _fill_ | _-_ | _-_ | _-_ | _-_ | _-_ | _fill_ |
