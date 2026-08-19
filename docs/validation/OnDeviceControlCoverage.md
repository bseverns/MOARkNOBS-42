# On-Device Control Coverage

This matrix tracks whether each major runtime feature has an on-device control path and OLED feedback path.

## Coverage Matrix

| Area        | Parameter / Function         | On-device control                                        | OLED feedback                                   | Coverage |
| ----------- | ---------------------------- | -------------------------------------------------------- | ----------------------------------------------- | -------- |
| Clock       | Clock source `EXT/INT`       | `Ctrl1+Ctrl4+Ctrl5`                                      | status text (`CLK SRC ...`)                     | Yes      |
| Clock       | Clock out enable             | `Ctrl1+Ctrl5`                                            | status text (`CLK OUT ...`)                     | Yes      |
| Profile     | Cycle profile A-D            | `Ctrl1+Ctrl2`                                            | status text (`PROFILE X`)                       | Yes      |
| Profile     | Save profile state           | long-press `Ctrl4` confirm, or config-mode exit autosave | status text (`Config Saved`)                    | Yes      |
| Profile     | Reload active profile        | long-press `Ctrl1` confirm                               | status text (`Profile Reset!`)                  | Yes      |
| Recovery    | Panic baseline reset         | `Ctrl0+Ctrl1+Ctrl2`                                      | status text (`Panic: Baseline`)                 | Yes      |
| Recovery    | Reload persisted config      | long-press `Ctrl3` confirm                               | status text (`Config Reloaded`)                 | Yes      |
| Config mode | Slot/type/channel/data edits | `Ctrl0+Ctrl2+Ctrl3+Ctrl5` then `Ctrl0..5`                | persistent config mode view                     | Yes      |
| Mapping     | Set slot to Note             | `Ctrl4+Ctrl5`                                            | status text (`Slot N: NOTE`)                    | Yes      |
| Mapping     | Set slot to Program Change   | `Ctrl3+Ctrl5`                                            | status text (`Slot N: PROG`)                    | Yes      |
| Mapping     | Set slot to Pitch Bend       | `Ctrl0+Ctrl5`                                            | status text (`Slot N: BEND`)                    | Yes      |
| Mapping     | Set slot to Aftertouch       | `Ctrl1+Ctrl4`                                            | status text (`Slot N: AFT`)                     | Yes      |
| Mapping     | Set slot to NRPN             | `Ctrl2+Ctrl5`                                            | status text (`Slot N: NRPN`)                    | Yes      |
| Mapping     | Set slot to RPN              | `Ctrl1+Ctrl3`                                            | status text (`Slot N: RPN`)                     | Yes      |
| Mapping     | Set slot to SysEx            | `Ctrl0+Ctrl3`                                            | status text (`Slot N: SYX`)                     | Yes      |
| LFO         | Select LFO 1/2               | LFO tune mode + `Ctrl0/1`                                | persistent LFO tune view                        | Yes      |
| LFO         | Enter/exit quick-tune mode   | `Ctrl0+Ctrl1+Ctrl3`, or `Ctrl5` to exit                  | persistent LFO tune view                        | Yes      |
| LFO         | Shape                        | LFO tune mode + `Ctrl2`                                  | persistent LFO tune view + status text          | Yes      |
| LFO         | Sync enable                  | LFO tune mode + `Ctrl3`                                  | persistent LFO tune view + status text          | Yes      |
| LFO         | Route target                 | LFO tune mode + `Ctrl4`                                  | persistent LFO tune view + status text          | Yes      |
| LFO         | Toggle live LFO 1 slot lane   | double-press `Ctrl5`                                     | status text (`LFO1 LIVE ON/OFF`)                 | Yes      |
| LFO         | Frequency                    | LFO tune mode + `CtrlPot0`                               | persistent LFO tune view + short status text    | Yes      |
| LFO         | Depth                        | LFO tune mode + `CtrlPot1`                               | persistent LFO tune view + short status text    | Yes      |
| LFO         | Sync ratio                   | LFO tune mode + `CtrlPot2` (sync ON)                     | persistent LFO tune view + short status text    | Yes      |
| LFO         | Bipolar / unipolar           | LFO tune mode + `CtrlPot2` (sync OFF)                    | persistent LFO tune view + short status text    | Yes      |
| Jitter      | Depth/smoothness base        | hold `Ctrl0+Ctrl3+Ctrl4` + `CtrlPot0/1`                  | persistent jitter tune view + short status text | Yes      |
| Arp         | Enable/disable assigned slot | `Ctrl2+Ctrl4`                                            | status text (`ARP ON/OFF/UNASSIGNED`)           | Yes      |
| Arp         | Base note bump               | `Ctrl2+Ctrl3`                                            | status text (`Arp Base ...`)                    | Yes      |
| Arp         | Swing presets                | hold `Ctrl2+Ctrl3`                                       | status text (`Swing: N%`)                       | Yes      |
| Arp         | Gate/octave edit             | hold `Ctrl2+Ctrl4` + `CtrlPot1/2`                        | control overlay (`Arp`)                         | Yes      |
| Reactive    | ARG method                   | `Ctrl0+Ctrl1`                                            | status text (`Slot N ARG=...`)                  | Yes      |
| Reactive    | ARG source pair              | `Ctrl0+Ctrl2`                                            | status text (`Slot N: EFx+EFy`)                 | Yes      |
| Reactive    | ARG enable/disable           | double-press `Ctrl4`                                     | status text (`ARG ON/OFF`)                      | Yes      |
| Reactive    | EF randomize/enable          | `Ctrl0+Ctrl4`                                            | status text (`EF turned ON`)                    | Yes      |
| Reactive    | EF oversampling preset       | double-press `Ctrl3`                                     | status text (`Slot N EF OS Nx`)                 | Yes      |
| System      | USB MIDI output              | `Ctrl3+Ctrl4+Ctrl5`                                      | status text (`USB MIDI ON/OFF`)                 | Yes      |
| System      | LED display mode             | `Ctrl3+Ctrl4`                                            | status text (`LED Mode ...`)                    | Yes      |
| Diagnostics | Enter/cycle pages            | long-press `Ctrl5` confirm                               | diagnostics page render                         | Yes      |

## Verification Notes

- Modal OLED views now dominate status overlays while active.
- LFO and jitter tuning both have persistent OLED mode views.
- Status overlays remain useful outside modal modes.
- Run `python3 tools/check_control_coverage.py --root .` after changing button combos or this matrix.
