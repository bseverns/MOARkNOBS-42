# Diagnostic Firmware Build Variants

The `teensy40_main_diag_*` PlatformIO environments are temporary bench bisects for isolating boot or runtime faults. They are not production firmware, release candidates, or substitutes for the normal automated and hardware-test gates.

Start by reproducing the problem with `teensy40_main`. Change one diagnostic boundary at a time, record whether the symptom changes, and return to the normal build after identifying the suspect lane.

```bash
pio run -d firmware -e <environment> -t upload
```

All variants use the conservative `POWER_CHOKED_V1` board profile. Variants marked with boot markers emit setup and first-loop progress over serial so a reset or hang can be localized.

## UI and Interrupt Bisects

| Environment | Disabled boundary | Use when |
| --- | --- | --- |
| `teensy40_main_diag_no_ui` | LED/OLED UI activity (`MN42_DIAG_DISABLE_LED_UI`) | A reset, hang, or rail symptom may track display or LED presentation work. |
| `teensy40_main_diag_no_timer1` | Timer1 MIDI ISR setup (`MN42_DIAG_DISABLE_TIMER1_ISR`) | A failure may be caused by timer ownership, ISR timing, or MIDI polling from the timer lane. |

## Scheduler-Tier Bisects

| Environment | Active scheduler boundary | Use when |
| --- | --- | --- |
| `teensy40_main_diag_no_schedulers` | No scheduler lanes are registered | Establish whether the failure exists before scheduled runtime work begins. |
| `teensy40_main_diag_high_only` | High priority only; mid and low disabled | Isolate MIDI, clock, modulation, and note-off work from UI/control scanning. |
| `teensy40_main_diag_mid_only` | Mid priority only; high and low disabled | Isolate serial ingest, command processing, and envelope/UI updates. |
| `teensy40_main_diag_low_only` | Low priority only; high and mid disabled | Isolate low-rate panel and maintenance work. |
| `teensy40_main_diag_no_low` | High and mid active; low disabled | Check whether low-rate work destabilizes an otherwise complete runtime. |
| `teensy40_main_diag_no_mid` | High and low active; mid disabled | Check whether serial/configuration or mid-rate UI work is responsible. |
| `teensy40_main_diag_no_high` | Mid and low active; high disabled | Check whether time-sensitive MIDI, clock, or modulation work is responsible. |

## Scheduler-Lane Bisects

These variants keep the low scheduler disabled and narrow high- or mid-tier work. Compare them against `teensy40_main_diag_no_low`.

| Environment | Additional disabled lanes | Use when |
| --- | --- | --- |
| `teensy40_main_diag_no_low_no_mid_serial` | Serial polling and command-queue processing | Determine whether the combined serial command path causes the symptom. |
| `teensy40_main_diag_no_low_no_poll_serial` | Serial polling only | Separate USB serial ingestion from already-queued command execution. |
| `teensy40_main_diag_no_low_no_command_queue` | Command-queue processing only | Separate command dispatch from USB serial ingestion. |
| `teensy40_main_diag_no_low_no_high_transport` | MIDI processing, pending note-offs, serial output service, internal clock, and arpeggiator update | Isolate high-tier transport/clock work from modulation work. |
| `teensy40_main_diag_no_low_no_high_modulation` | LFO and envelope-follower processing | Isolate high-tier modulation work from transport/clock work. |

## Setup and Boot Bisects

| Environment | Boot behavior | Use when |
| --- | --- | --- |
| `teensy40_main_diag_boot_markers` | Full runtime with boot markers | Locate the last completed setup or first-loop stage without removing functionality. |
| `teensy40_main_diag_no_runtime` | Modes and UI initialize; runtime initialization is skipped; boot markers enabled | Determine whether the fault begins inside `initializeRuntime()`. |
| `teensy40_main_diag_setup_only` | Runtime initialization and standalone loop body skipped; boot markers enabled | Check whether setup alone is stable. |
| `teensy40_main_diag_setup_no_modes` | Modes, runtime, and standalone loop body skipped | Isolate mode/profile initialization from earlier setup. |
| `teensy40_main_diag_setup_no_ui` | UI, runtime, and standalone loop body skipped | Isolate all UI initialization from earlier setup. |
| `teensy40_main_diag_setup_no_leds` | LED initialization, runtime, and standalone loop body skipped | Separate LED bring-up from display and button initialization. |
| `teensy40_main_diag_setup_no_display` | Display initialization, runtime, and standalone loop body skipped | Separate OLED bring-up from LED and button initialization. |

## Interpreting Results

A symptom disappearing identifies a suspect boundary, not a proven root cause. Confirm it with the next narrower variant and capture:

- the exact environment and Git SHA;
- power source and board/rework profile;
- the last serial boot marker or diagnostic line;
- whether USB enumeration, OLED, LEDs, MIDI, and controls remained responsive;
- the corresponding result from `teensy40_main`.

Do not ship a diagnostic environment to work around a fault. Restore the normal build, fix the underlying lane, and run the release-readiness and hardware-test gates.
