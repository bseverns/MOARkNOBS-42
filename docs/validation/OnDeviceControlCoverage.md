# On-Device Control Coverage

This matrix tracks whether each major runtime feature has an on-device control path and OLED feedback path.

## Coverage Matrix

| Area        | Parameter / Function         | On-device control                                        | OLED feedback                                   | Coverage |
| ----------- | ---------------------------- | -------------------------------------------------------- | ----------------------------------------------- | -------- |
| Clock       | Clock source `EXT/INT`       | `Ctrl1+Ctrl4+Ctrl5`                                      | status text (`CLK SRC ...`)                     | Yes      |
| Clock       | Clock out enable             | `Ctrl1+Ctrl5`                                            | status text (`CLK OUT ...`)                     | Yes      |
| Profile     | Cycle profile A-D            | `Ctrl1+Ctrl2`                                            | status text (`PROFILE X`)                       | Yes      |
| Profile     | Save profile state           | long-press `Ctrl4` confirm, or config-mode exit autosave | status text (`Config Saved`)                    | Yes      |
| Profile     | Reload profile               | long-press `Ctrl1` confirm                               | status text (`Profile Reset!`)                  | Yes      |
| Recovery    | Panic baseline reset         | `Ctrl0+Ctrl1+Ctrl2`                                      | status text (`Panic: Baseline`)                 | Yes      |
| Config mode | Slot/type/channel/data edits | `Ctrl0+Ctrl2+Ctrl3+Ctrl5` then `Ctrl0..5`                | persistent config mode view                     | Yes      |
| LFO         | Select LFO 1/2               | LFO tune mode + `Ctrl0/1`                                | persistent LFO tune view                        | Yes      |
| LFO         | Shape                        | LFO tune mode + `Ctrl2`                                  | persistent LFO tune view + status text          | Yes      |
| LFO         | Sync enable                  | LFO tune mode + `Ctrl3`                                  | persistent LFO tune view + status text          | Yes      |
| LFO         | Route target                 | LFO tune mode + `Ctrl4`                                  | persistent LFO tune view + status text          | Yes      |
| LFO         | Frequency                    | LFO tune mode + `CtrlPot0`                               | persistent LFO tune view + short status text    | Yes      |
| LFO         | Depth                        | LFO tune mode + `CtrlPot1`                               | persistent LFO tune view + short status text    | Yes      |
| LFO         | Sync ratio                   | LFO tune mode + `CtrlPot2` (sync ON)                     | persistent LFO tune view + short status text    | Yes      |
| LFO         | Bipolar / unipolar           | LFO tune mode + `CtrlPot2` (sync OFF)                    | persistent LFO tune view + short status text    | Yes      |
| Jitter      | Depth/smoothness base        | hold `Ctrl0+Ctrl3+Ctrl4` + `CtrlPot0/1`                  | persistent jitter tune view + short status text | Yes      |
| Arp         | Enable/disable               | `Ctrl2+Ctrl4`                                            | status text (`ARP ON/OFF`)                      | Yes      |
| Arp         | Swing presets                | hold `Ctrl2+Ctrl3`                                       | status text (`Swing: N%`)                       | Yes      |
| Arp         | Gate/octave edit             | hold `Ctrl2+Ctrl4` + `CtrlPot1/2`                        | control overlay (`Arp`)                         | Yes      |
| Reactive    | ARG method                   | `Ctrl0+Ctrl1`                                            | status text (`Slot N ARG=...`)                  | Yes      |
| Reactive    | ARG source pair              | `Ctrl0+Ctrl2`                                            | status text (`Slot N: EFx+EFy`)                 | Yes      |
| Diagnostics | Enter/cycle pages            | long-press `Ctrl5` confirm                               | diagnostics page render                         | Yes      |

## Verification Notes

- Modal OLED views now dominate status overlays while active.
- LFO and jitter tuning both have persistent OLED mode views.
- Status overlays remain useful outside modal modes.
