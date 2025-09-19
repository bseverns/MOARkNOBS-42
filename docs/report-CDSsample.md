---
title: "MOARkNOBS-42: An Instrument That Publishes Its Thinking"
author: "Ben Severns"
date: 2025-09-17
version: "v0.1.0 (tag: init)"
repository: "https://github.com/bseverns/MOARkNOBS-42"
tags: ["open hardware", "critical digital practice", "latency", "mapping", "documentation", "pedagogy"]
word_count_estimate: ~2000
shorttitle: "MN42 Instrument Report"
header-includes:
  - |
    \usepackage{fancyhdr}
    \pagestyle{fancy}
    \fancyhead{}
    \fancyfoot{}
    \fancyhead[LE,RO]{\textsc{MN42 Instrument Report}}
    \fancyfoot[CE,CO]{\textsc{MOARkNOBS-42}}
    \fancyfoot[LE,RO]{\thepage}
---

::: {#cover}
# MOARkNOBS-42: An Instrument That Publishes Its Thinking

**Author**: Ben Severns  
**Date**: 2025-09-17  
**Version**: v0.1.0 (tag: `init`)  
**Repository**: [github.com/bseverns/MOARkNOBS-42](https://github.com/bseverns/MOARkNOBS-42)  
**Release DOI**: Zenodo deposition requested for tag `init` (pending DOI assignment)

## Abstract

MOARkNOBS-42 (MN42) is an open, reproducible MIDI instrument built so musicians, educators, and critics can audit intent as well as sound. Forty-two controls, a Teensy 4.0 brain, and firmware documented in editable manifests make the layout forkable instead of opaque. Every release ships with the assumption ledger, mapping manifest, and latency laboratory so claims about feel, access, and ethics stay interrogable. The public repo stores the rig scripts and raw CSVs for latency (`docs/bench/latency/latency.csv`) and ADC noise (`docs/bench/noise/adc_idle.csv`) beside their methods notes, keeping evidence linked to prose. This report traces the project’s thesis—documentation as playable critique—alongside the architecture that ships, the tests that defend it, and the kinds of unruly practice MN42 invites. Readers get enough bench detail to reproduce numbers, enough mapping detail to rebuild agency, and enough narrative to place MN42 inside critical digital instrument making.
:::

\newpage

---

## 1) Thesis: Instruments are Arguments {#sec-thesis}

Every controller is a position paper in disguise. Knob counts, scanning rates, curves, and modes are **claims** about who gets to play, how quickly bodies can be heard, and what counts as *good noise*. MN42 states three things up front:

1. **Transparency is a feature.**  The build, the mappings, and the measurements are public in *preferred editable forms*. If you disagree with a choice, the project makes it easy to change it and report back.
2. **Latency and variance matter to dignity.**  Timing isn’t just numbers—it’s how a sound greets a gesture. MN42 treats **stability** (low jitter) and **clarity** (documented mappings) as first-class affordances.
3. **Documentation is part of the art.**  Assumption ledgers, change logs, and test rigs aren’t clerical afterthoughts; they are the scaffolds that let students and strangers build new work without asking permission.

MN42, in other words, is a **playable essay** about control and accountability.

---

## 2) Architecture & Release: What Ships and Why {#sec-release}

MN42 is a Teensy-based, 42-control instrument intended for both stage and studio. The release is structured so that a motivated builder can go from curiosity → build → critique without dead ends.

**What ships (at a glance)**

- **Hardware package**
  - CAD/PCB sources and fabrication files; annotated BOM with part substitutions and notes on supply volatility
  - Panel layout (top-down) reflecting the control grammar (clusters, travel, performance reach)
- **Firmware package**
  - Scanning & debouncing, value mapping utilities (curves, clipping, slew), preset/mode system
  - MIDI I/O options (USB, DIN, TRS Type-A), clock utilities, and a small test harness
- **Documentation & accountability**
  - `ASSUMPTIONS.md` — statement-→risk-→mitigation table (e.g., “local processing,” “expected buffer sizes,” “no network calls”)
  - `MAPPING_MANIFEST.md` — a machine-readable table of control → parameter relationships (domain, curve, range, smoothing, notes)
  - `MEASUREMENT.md` — replicable steps for measuring round-trip latency and variance with a loopback rig (plus templates for plots)
  - `CHANGELOG.md` — human-readable deltas with a bias toward “why” over “what”
  - `TEACHING_NOTES.md` — lab prompts, rubrics, accessibility moves

**Release philosophy**

- **Rebuild, don’t revere.**  Files are released in editable formats (not just exports).
- **Fork friction is design work.**  Defaults are opinionated but documented so that disagreement becomes a pull request, not a dead end.
- **Failure is a teacher.**  Known limitations and “gotchas” live near the README, not buried in issues.

---

## Methods in Brief {#sec-methods}

- **Build stack** — PlatformIO project targeting Teensy 4.0 (`platformio.ini`) with reproducible dependency locks inside `firmware/lib/`. Firmware builds via `pio run -e teensy40_main`; tagged releases bundle the compiled `.hex` alongside the change log.
- **Hardware & rig** — Controller assembled per `hardware/README.md`; latency bench uses a loopback audio interface, Teensy USB MIDI in USB_MIDI_SERIAL mode, and the measurement scripts catalogued in `docs/bench/latency/oblique_rtl.md`. ADC noise benches plug the front-end into the quiet enclosure documented in `docs/bench/noise/method.md`.
- **Reproducing numbers** — Step-by-step recipes and environment captures live in `docs/bench/README.md`, `docs/bench/environment/conditions.json`, and per-test `method.md` files. Each bench directory keeps its raw CSV exports (`latency.csv`, `adc_idle.csv`) alongside the plots so the PDF, numbers, and scripts travel together.
- **Continuous checks** — `./test.sh` exercises Unity firmware tests (`pio -d firmware test -e teensy40_unity -vvv`) and the bridge suite, while manual QA steps follow `docs/qc-checklist.md`. Section \ref{sec-measurement} cross-references the exact bench notes for data cited in this report.

---

## 3) Control Topography & Mapping Grammar {#sec-mapping}

Forty-two is a lot—on purpose. The surface invites **poly-attention**: one hand can ride a macro while the other hand shades the room with small bias moves. MN42’s mapping grammar is explicit so players (and students) can learn the thing, break the thing, and build a different thing with eyes open.

**Mapping strategies MN42 supports (and documents)**

- **1→1 monotonic** — one control, one parameter, declared curve (lin/exp/log), bounds, and smoothing (ms)
- **1→N fan-out** — one control moves multiple targets with weights (e.g., cutoff + env depth + send A)
- **M→1 fusion** — multiple controls (or modes) converge on one target (e.g., crossfader + bias)
- **Stateful / temporal** — mappings that depend on mode or history (e.g., long-press to re-range; latched scenes)
- **Time affordances** — tap-tempo, quantized latching, “safe catch” for grabbed encoders

**Excerpt — mapping manifest (Table \ref{tab:mapping-manifest})**

\begin{table}[htbp]
  \centering
  \caption{Mapping manifest excerpt showcasing mixed mapping strategies.}
  \label{tab:mapping-manifest}
  \begin{tabular}{rlllll}
    \textbf{Control} & \textbf{Target(s)} & \textbf{Curve} & \textbf{Range} & \textbf{Slew (ms)} & \textbf{Notes} \\
    \hline
    K07 & Filter cutoff & exp & 80--8000 Hz & 8 & fast, percussive \\
    K07 & Env depth ($w=0.35$) & lin & 0.0--0.8 & 8 & 1→N macro shade \\
    F02+K12 & Send A (fusion) & lin & 0.0--1.0 & 12 & crossfade + bias \\
    B03 & Scene latch (quant.) & step & 1 bar grid & --- & temporal affordance \\
  \end{tabular}
\end{table}

This grammar is as much **pedagogy** as it is engineering: it lets a class talk concretely about agency, intention, and design trade-offs without mystique, and the full manifest remains editable in the repo for forkable mappings.

---

## 4) Tuning for Feel: Measurement Without Myth {#sec-measurement}

MN42 includes a small *latency & stability lab* so that “it feels tight” can be tested, reproduced, and taught. The rig is humble: an audio interface, a loopback, and a script to compute **round-trip latency** and **variance** over many trials. The goal isn’t hero numbers; it’s **clarity**. Bench evidence cited here references the latency and noise benches documented in `docs/bench/latency/method.md`, `docs/bench/latency/oblique_rtl.md`, and `docs/bench/noise/method.md`, with raw CSVs co-located for inspection.

**What the lab provides**

- **Procedure** — step-by-step loopback method, buffers to test, recommended trial counts
- **Reporting** — table templates for mean, standard deviation, and 95% confidence bands (see Section \ref{sec-methods} for how to regenerate and extend them)
- **Comparisons** — plots for baseline vs “tuned” machine states (e.g., smaller buffers, pinned drivers)
- **Notes** — how mappings and envelopes change perceived delay (fast attacks reveal; pads forgive)

**Why this matters to art**

- If variance is high, MN42 defaults to **macro-friendly** presets (1→N) that de-emphasize split-second attacks.
- If variance is low, presets unlock **micro-timbral** control (tight percussive starts, short envelopes).
- In a mixed-hardware classroom, the manifest advertises which presets are **“friendly to older laptops.”**

Numbers don’t dictate style; they **shape kindness**.

---

## 5) Scenes: Wild Work MN42 Wants to Enable

MN42 is released to sponsor unruly, generous uses—work that treats the controller as a collaborator rather than a meter stick. Here are the kinds of scenes this hardware is designed to make possible:

- **Noise quilts & ensemble conduction**
  One MN42 acts as a **conductor** for small ensembles: macros push shared gestures (density, grain, brightness) while per-player bias knobs restore agency. The mapping manifest makes the social contract legible: what the conductor controls, what the players keep.

- **Generative geometries with accountability**
  MN42 drives a live A/V patch where **four shape families** are animated by audio features and controller motion. A toggle overlays the mapping routes as lines, so the audience (and students) can read how signal becomes image, not just watch it.

- **Site-responsive rooms**
  A preset turns MN42 into a **room instrument**—slow macros for color and texture, quick faders for “who’s in front.” The scene encourages shared authorship: a gallery guard, a visitor, and a performer can all “nudge” the same mix without stepping on each other.

- **Score-by-seed**
  MN42 plants **seeds** (rule bundles) that grow rhythms and textures; a **ledger** logs re-seeds, freezes, and macro moves. Performers can hand someone else their recipe and say, “grow it differently.”

- **Teaching as performance**
  In critique, students perform a 60-second study on two presets—*Percussive* and *Pad*—and pair it with a tiny results table. The performance is an argument: here’s what my numbers imply, here’s where my mapping disagrees, here’s what I chose.

Across all of these: the instrument keeps **receipts** so the work stays reproducible, debatable, and teachable.

---

## 6) Public Practice: Ethics, Access, and Classroom Use

MN42 treats **ethics as an interface**. That means:

- **Assumption Ledger** (living)
  For each design claim (e.g., “local processing,” “no network calls,” “sub-10 ms on tuned rigs”), the ledger lists **risk** and **mitigation**, with pointers to tests or plots. It’s not a boast list; it’s a map of **where we could be wrong**.

- **Accessibility by default**
  Knob spacing, hand reach, legible labels, and low-effort firmware flashing are design constraints, not stretch goals. Teaching notes include **low-bandwidth alternatives** for students with intermittent hardware and **bilingual scaffolds** when materials leave English.

- **Reproducible learning**
  A two-week lab has students measure latency on their own machine, annotate two presets, and publish a small report. Rubrics emphasize **method** (how you knew), **craft** (how you tuned), **accountability** (what you documented), and **reflection** (what changed).

- **Community invitations**
  The release asks for **user recipes** (mapping manifests with short notes), **bug diaries** (what broke, how you unbroke it), and **ethical edge cases** (where assumptions failed in the wild). The project values *differences*, not just optimizations.

---

## References

- **Latency & control intimacy.** Wessel, D., & Wright, M. *Problems and Prospects for Intimate Musical Control of Computers* (NIME).
  Classic statement placing an acceptable upper bound on audible reaction at ~10 ms; details control strategies.
- **Latency measurement practice.** Oblique Audio. *RTL Utility* (site + user guide).
  Standardized loopback procedure for round-trip latency.
- **Technical overview / practitioner guidance.** *Sound on Sound*: *Round Trip Latency (RTL), J-Scope Oscilloscope*.
  Accessible discussion of real-world RTL vs reported values.
- **Perception/JND.** Schmid, A. et al. *Measuring the Just Noticeable Difference for Audio Latency* (ACM IMX 2024).
  JND depends on base latency and task; users can perceive ≈10 ms in some contexts; benefits <25–50 ms vary by task.
- **Mapping strategies.** Hunt, A., & Kirk, R. *Mapping performer parameters to synthesis engines* / *Mapping Strategies for Musical Performance*.
  Foundational taxonomy and design implications for learnability/expressivity.
- **Open hardware definitions & practice.** OSHWA: *Open Source Hardware Definition*; *Best Practices for Sharing*.
  Definition and packaging guidance for reproducible hardware.
- **Privacy by Design (for sensing builds).** Cavoukian, A. *The 7 Foundational Principles of Privacy by Design*.
  Design-stage guidance for consent-forward systems.
- **Embodied interface practice.** Bongers, B. *Physical Interfaces in the Electronic Arts: Interaction Theory and Interfacing Techniques* (Trends in Gestural Control of Music, 2000).
  Frames bodily feedback loops and sensing categories for DMI builders.
- **Evaluation heuristics.** Wanderley, M. M., & Orio, N. *Evaluation of Input Devices for Musical Expression* (Computer Music Journal, 2002).
  Offers criteria and methodologies for assessing expressivity, learnability, and control intimacy.
- **Pedagogical DMI design.** Gurevich, M., & Cavan Fyans, A. *Digital Musical Interactions: Performer–System Relationships in the Studio and Classroom* (NIME 2011).
  Discusses participatory design tactics for inclusive, teachable instrument ecologies.

## Appendices

**A. Builder’s Quick-Look**
- Build level: intermediate (soldering + flashing + basic DAW literacy)
- Recommended interface: any stable USB audio with known buffer control
- Useful tools: multimeter, small logic probe, breadboard for add-ons

**B. File Index (repo-root)**
- `/hardware/` — CAD/PCB, fabrication files, panel layout
- `/firmware/` — source, presets, tests
- `/docs/ASSUMPTIONS.md` — living ledger
- `/docs/MAPPING_MANIFEST.md` — machine-readable map
- `/docs/MEASUREMENT.md` — latency & stability lab
- `/docs/TEACHING_NOTES.md` — lab prompts, rubrics, accessibility

**C. Changelog (excerpt)**
- `v0.3.x` — Added mapping manifest export; revised pad preset to be variance-tolerant
- `v0.2.x` — Introduced loopback lab; added histogram/CI templates
- `v0.1.x` — Initial public release; panel map; baseline presets

---

# Closing

MN42 isn’t trying to be the fastest, the flashiest, or the most knobs per dollar. It’s trying to be **the most accountable invitation**—a controller that tells you what it’s doing, lets you alter the terms, and makes space for art that wouldn’t have happened without a little extra **clarity**. If you make something wild with it, the instrument wants to learn *from you*, and the release is built to listen.
