# MOARkNOBS-42 — Assumption Ledger  
*v0.3.0-portfolio-2025 · Last updated: [2025-09-13]*

> What we believe is true, why it matters, how we test it, and what we’ll do if it’s false.

## 1) ADC resolution & scan cadence
- **Assumption:** The control ADC reads at **[12-bit]** resolution with an effective scan cadence of **[≥1 kHz]** across **[N]** analog channels (after multiplexing).  
- **Why it matters:** Sets the ceiling for smoothness and end-to-end latency.  
- **Test:** Log raw counts per channel for **10 s**; report min/median/max inter-sample interval and effective ENOB (histogram).  
- **Fallback if false:** Reduce channel batch size; stagger scans; or oversample + decimate to hit **[target ≤10 ms]** E2E latency.  
- **Status:** ☐TBD ☐Confirmed

## 2) Analog conditioning & noise floor
- **Assumption:** With RC input filtering at **[τ = … ms]**, the **RMS noise** per channel is **[≤ 2 LSB]** at rest; no cross-talk above **[-60 dB]** between adjacent controls.  
- **Why it matters:** Prevents value “shimmer” and false updates.  
- **Test:** Scope/FFT a parked pot; log standard deviation and adjacent-channel correlation.  
- **Fallback:** Increase τ slightly; add digital IIR smoothing α = **[0.1–0.2]**; verify no perceptible lag.  
- **Status:** ☐TBD ☐Confirmed

## 3) Hysteresis / deadband for stable intent
- **Assumption:** Per-control **deadband** of **[1–2 LSB]** (or **[±0.5%]**) prevents chatter while preserving fine moves.  
- **Why it matters:** Avoids spurious MIDI/OSC floods; keeps “hands-off” stable.  
- **Test:** Step inputs in **1 LSB** increments; confirm message emission only when change > deadband.  
- **Fallback:** Tune deadband per control; add “fine mode” switch for high-resolution edits.  
- **Status:** ☐TBD ☐Confirmed

## 4) Message rate-limits & back-pressure
- **Assumption:** Each control is capped at **[≤120 msgs/s]** with a global bus cap of **[≤1000 msgs/s]**; bursts are coalesced (last-value wins).  
- **Why it matters:** Prevents DAW/synth overflow; keeps USB/MIDI stable.  
- **Test:** Spin one and many knobs at max speed; measure outgoing rate and dropped/coalesced counts.  
- **Fallback:** Increase per-control holdoff **[≥8 ms]**; add priority to active touches.  
- **Status:** ☐TBD ☐Confirmed

## 5) Resolution & mapping (7-bit / 14-bit / OSC)
- **Assumption:** Default MIDI CC is **7-bit (0–127)**; optional **14-bit** via MSB/LSB pairs; OSC path emits **float 0.0–1.0**.  
- **Why it matters:** Ensures predictable feel and cross-tool compatibility.  
- **Test:** Sweep full throw; verify linearity, end-stops, and no wrap/skip; confirm DAW learns MSB/LSB as a pair.  
- **Fallback:** Provide per-control curve (log/exp) and clamp/scale tables; document host quirks.  
- **Status:** ☐TBD ☐Confirmed

## 6) Power, grounding, and noise coupling
- **Assumption:** 5V supply headroom **[≥500 mA]**; star-ground layout; analog ground isolated from digital returns to **[spec]**; no audible PSU ripple in audio chains.  
- **Why it matters:** Avoids random jumps and audio contamination.  
- **Test:** Load-step PSU; observe ADC noise; probe for ground loops; ear-test with high-gain chain.  
- **Fallback:** Add LC filter on 5V rail; split ground planes; ferrites on USB and encoder lines.  
- **Status:** ☐TBD ☐Confirmed

## 7) Firmware resilience & latency budget
- **Assumption:** Event-driven scan → normalize → threshold → queue → transmit; watchdog enabled; worst-case knob-to-MIDI/OSC latency **[≤10 ms]**, typical **[3–5 ms]**.  
- **Why it matters:** Feel. Latency is the instrument.  
- **Test:** Timestamp at scan and at TX; compute distribution over **60 s** under load (many controls + USB).  
- **Fallback:** Move heavy math out of ISR; raise TX buffer; pin high-priority tasks.  
- **Status:** ☐TBD ☐Confirmed

## 8) Mapping persistence & serviceability
- **Assumption:** User mappings persist to **[EEPROM/Flash]** as a human-readable record (e.g., JSON/TOML) with **rollback** to last-known-good; no JTAG required for remap.  
- **Why it matters:** Real users change rigs; support shouldn’t require firmware rebuilds.  
- **Test:** Save/restore cycles **[≥100]**; power-pull mid-write → confirm rollback; verify import/export path.  
- **Fallback:** Add double-buffered config pages; expose “factory reset” combo; ship a minimal mapping editor.  
- **Status:** ☐TBD ☐Confirmed

### “Never Do” (design guardrails)
- No telemetry; no hidden data capture.  
- No keystroke emulation without explicit firmware build flag.  
- No vendor-locked software requirements for core functions.  
- No undocumented mappings; every control must be legible in the parameter map.

### Quick Bench Checklist (5 minutes before a demo)
1. Purge power up; check **BUILD / FW version** on boot.  
2. Spin three adjacent knobs: verify **no cross-talk** and **no shimmer** at rest.  
3. Max-spin one control: confirm **rate-limit**; DAW remains responsive.  
4. Save a new mapping; power-pull; confirm **persistence** and rollback safety.  
5. Measure **latency** with timestamps; stay within **[≤10 ms]**.
