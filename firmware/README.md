# MOARkNOBZ MN42 MIDI Controller

## Introduction
The **MOARkNOBZ MN42** is a flexible, dynamic MIDI controller designed for real-time performance and parameter modulation. Originally designed with **42 physical potentiometers**, the current hardware configuration replaces them with:

- A **single physical pot** for continuous control.
- A **set of buttons** to navigate and modify **42 virtual CC slots**.
- An **OLED display** and **addressable LEDs** for real-time feedback.

While the hardware layout has changed, the firmware still operates as though there are **42 virtual pots (CC slots)**. Each slot can be individually assigned:
- A **MIDI Channel**
- A **CC number**
- **Optional Envelope Follower modulation**

With **one pot to rule them all**, you can swap between these 42 slots on the fly, using the button matrix. The ButtonManager interprets short, long, and multi-button presses, allowing for dynamic control without requiring a massive bank of physical knobs.

---

## Features Overview

### Virtual Potentiometers
- The firmware manages **42 CC slots**.
- The **physical pot** controls one virtual slot at a time, with quick selection via buttons.
- Pot positions update dynamically, making it easy to recall and adjust values live.

### Button-Controlled System
- **Short, long, and double press detection** for quick access.
- **Multi-button combinations** to unlock additional features.
- **Dedicated commands for slot selection, channel/CC assignment, and effect toggling.**

### Envelope Follower (EF)
- Dynamically modulates MIDI CC values based on **incoming audio amplitude**.
- Choose between **Standard Envelope Follower (SEF) and Advanced Relative Gain (ARG) mode**.
- Multiple filter types: **Lowpass, Highpass, Bandpass, Linear, Opposite, Exponential, Random**.
- Adjustable response behavior using onboard **filter tuning pots** for Lowpass, Highpass, and Bandpass.

### Real-Time Filter Tuning
- Two dedicated tuning potentiometers allow **live control** over filter settings:
  - **Frequency Control** (FILTER_FREQ_POT_PIN): Adjusts cutoff frequency.
  - **Resonance Control** (FILTER_RES_POT_PIN): Controls the resonance, shaping sharpness.
- Active only when the **Envelope Follower is assigned and using Lowpass, Highpass, or Bandpass mode**.
- LED and OLED display reflect real-time adjustments.

### Display & LED Feedback
- **OLED screen**: Displays active slot, envelope status, channel/CC info.
- **Addressable LEDs**: Provide color-coded feedback on the current mode and modulation state.
- Dynamic updates for envelope follower activity and real-time performance.

### MIDI Connectivity
- Supports **USB MIDI** (DAW, softsynths, etc.).
- Supports **DIN MIDI** (hardware synths, sequencers, etc.).
- **Both outputs work simultaneously**, so you can control multiple systems at once.

### EEPROM Storage
- **Saves your mappings, assignments, and filter settings across power cycles**.
- Prevents accidental resets when tweaking your setup.

### Web-Based Editor
- **An HTML-based firmware editor** will allow quick reconfiguration over USB.
- Users will be able to set CC assignments, LED color schemes, and envelope behaviors visually. -url tbd-

---

## Getting Started

### 1. Setup & Connections
- **Connect the MN42** via USB to a computer.
- **Power Requirements:** If using external power, supply a stable **5V 2A**.
- **MIDI Connections:**
  - **USB MIDI**: Works immediately with most DAWs and software.
  - **DIN MIDI**: Connect external gear via the **MIDI IN/OUT ports**.
- Upon connection, your device should be detected as **"MOARkNOBZ"**.

### 2. Powering On & Interface Overview
- The **OLED display** will show a brief **MOAR splash screen**.
- The **firmware loads stored assignments from EEPROM**.
- LEDs will indicate:
  - **Red**: Active slot.
  - **Green**: Envelope Follower is enabled.
  - **Blue**: Special configuration or status modes.

### 3. Controlling Virtual Pots
- The single **physical pot** controls whichever **virtual CC slot** is active.
- Use buttons to navigate between slots, assigning them to different MIDI parameters.
- The OLED screen and LED color scheme reflect changes instantly.

---

## Using the Controls

### 1. The Physical Pot
- **Adjusts MIDI CC values** in real-time.
- The **OLED and LED indicators** reflect active slot status.
- **Easily reassign the pot** to any virtual CC slot using button commands.

### 2. Filter Tuning Controls
#### When Are the Tuning Pots Used?
- Only active when an **Envelope Follower is assigned**.
- Only affects slots using **Lowpass, Highpass, or Bandpass filters**.
- Changes take effect **immediately** and persist after saving.

#### How to Use the Filter Controls
1. **Enable an Envelope Follower** on a slot.
2. **Set the filter type** (Lowpass, Highpass, or Bandpass) using button combinations.
3. **Turn the tuning pots:**
   - **Frequency Pot:** Controls the cutoff point, shifting timbre in response to input.
   - **Resonance Pot:** Adjusts the filter resonance (sharpness or smoothness of effect).
4. **Watch the LEDs** to monitor changes—color intensity reflects filter activity.

#### Creative Uses
- **Sculpt dynamic modulations** by fine-tuning envelope-driven MIDI CC outputs.
- **Create rhythmic sweeps** by assigning the Envelope Follower to LFO-like behaviors.
- **Introduce subtle or dramatic resonance shifts** based on real-time audio input.
- **Enhance live performance** with responsive and expressive filtering.

### 3. Buttons & ButtonManager
#### Button Functions
| Button | Short Press | Long Press | Double Press |
|--------|------------|------------|--------------|
| **#0** | Toggle Envelope Follower | Assign slot to EF | Cycle EF Filter Type (Forward) |
| **#1** | Select next slot | | Cycle EF Filter Type (Backward) |
| **#2** | Cycle EF assignment | | |
| **#3** | Cycle active slot’s MIDI Channel (1-16) | | |
| **#4** | Cycle active slot’s CC number | Reset EEPROM | Save Configuration |
| **#5** | Tap BPM | | |

#### Multi-Button Combos
| Combo | Action |
|-------|--------|
| **#0 + #1** | Cycle EF ARG mode method |
| **#2 + #3** | Cycle light modes |
| **#4 + #5** | Turn on EF (if off) and randomly assign envelope |

### 4. Display & LED Feedback
- The **OLED screen** provides clear, at-a-glance status updates.
- **LED indicators** shift dynamically based on envelope status, slot selection, and modulation depth.

---

## Troubleshooting & Tips

| Issue | Possible Cause | Solution |
|-------|---------------|---------|
| Device not recognized | Faulty USB cable | Try a different cable/port |
| No MIDI output | Incorrect slot or CC mapping | Verify MIDI assignments |
| Envelope not responding | EF not enabled | Check if EF mode is active |
| Filter controls not working | Wrong filter type selected | Ensure LPF/HPF/BPF is active |
| EEPROM settings not saving | Not stored properly | Use the **Save Configuration** button |

---

## Firmware Updates & Future Expansions
- The upcoming **HTML-based configuration editor** will streamline CC assignments and visual mapping.
- Future firmware updates will introduce **custom modulation curves** and additional LED behaviors.

---

## Contact & Support
For firmware updates, detailed documentation, or troubleshooting:
- Visit **our GitHub repository**
- Contact **support@bseverns.me**

Enjoy the **MOARkNOBZ** experience—where one pot holds the power of **42 virtual CC slots**, advanced modulation, and expressive control. Happy tweaking!