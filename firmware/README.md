MOARkNOBZ MN42 MIDI Controller

Introduction
The MOARkNOBZ MN42 is a flexible, dynamic MIDI controller designed for real-time performance and parameter modulation. The original design featured 42 physical potentiometers, but your current hardware configuration substitutes those 42 pots with:

A single physical pot for continuous control.
A set of buttons to select or modify which “virtual CC slot” (among 42) the single pot is controlling.
Under the hood, the firmware still supports 42 “virtual pots” (or CC slots) that can each be configured with a MIDI Channel, CC number, and optional Envelope Follower modulation. The physical pot controls whichever “virtual slot” is currently active, and the ButtonManager handles button-based input for everything from slot selection to toggling the Envelope Follower.

1. Features Overview
42 Virtual CC Slots
Internally, the firmware can manage 42 CC “slots,” each mapped to a MIDI Channel/CC. Though you see only 1 physical pot, you can assign it to any of these 42 slots.

Single Physical Pot
Adjust MIDI CC values in real-time. The pot’s assignment can be changed (e.g., you might set it to CC Slot #0, then switch it to #15, etc.).

Multiple Momentary Buttons

Used to change the “active slot” the pot controls, toggle Envelope Follower modes, trigger saving/loading, etc.
Support for short press, long press, double press, and multi-button combos (all managed by the ButtonManager).
Envelope Follower
Dynamically modulate CC values based on incoming audio amplitude. Choose from standard or combined “ARG” modes, with optional filter types (Lowpass, Highpass, Bandpass, etc.).

OLED Display
Shows the active slot, envelope status, channel/CC info, plus short status messages for button actions.

Addressable LEDs
Provide color-coded feedback for active slot, envelope activity, mode selection, etc.

MIDI over USB and DIN (hardware serial)
Connect to computers, DAWs, synth modules, or other MIDI-capable gear.

EEPROM Storage
Remembers your assignments (slot → channel/CC), Envelope Follower settings, and LED brightness on reboot.

Configurable via USB serial interface & planned HTML editor
Quickly reassign all 42 CC slots, save custom configurations, etc.

2. Getting Started
2.1 Unboxing and Setup
Connect the MN42 to your computer via USB.
For external power (if required), use a stable 5V 2A supply.
Ensure your operating system or DAW detects the device as “MOARkNOBZ.”
(Optional) If you have external MIDI hardware, connect MIDI cables to the DIN ports (IN/OUT).

2.2 Powering On
On boot, the OLED typically displays “MOAR” or a brief splash screen.
The firmware loads any saved settings from EEPROM (slot assignments, envelope states, etc.).
The LEDs may flash or show default indications (e.g., active slot in red, envelope mode in green if active).

2.3 Configuring MIDI Channels and CCs
By default, the system might assign all 42 slots to Channel 1, with CC numbers 0–41.
Button-based: Use short/long/double presses to move between slots, set channels, or map the Envelope Follower.
Serial/HTML Editor: If you prefer bulk edits, connect via USB serial or the upcoming firmware editor website to reassign channels/CCs quickly.

3. Using the Controls
3.1 Physical Pot
Single Pot: Turning this pot sends MIDI CC messages for whichever virtual slot is currently active.
The LED feedback and display will update to reflect the pot’s current value or “slot-based” color coding.
You can switch the pot’s assignment to a different slot at any time (see the ButtonManager details below).

3.2 Buttons & ButtonManager
Your device has multiple momentary buttons—some may be dedicated to system-level controls (like toggling Envelope Follow mode), while others might serve to select the active slot. These button presses are handled by the ButtonManager class, which:

Reads & Debounces each button (either direct pins or via a multiplexer).
Maintains a state machine per button to detect short press, long press, double press, or simultaneous multi-button presses.
Calls the appropriate action—e.g. toggle Envelope Follower, save to EEPROM, select next CC slot.

Common Button Press Actions:

Short Press (Control Button #0): Toggle Envelope Follower On/Off.
Short Press (Control Button #1): Select the next slot for the pot (Slot #0 → Slot #1, etc.).
Short Press (Control Button #2): Cycle Envelope to follow [if EF on]
Short Press (Control Button #3): Cycle the active slot’s out MIDI channel from 1-16.
Short Press (Control Button #4): Cycle the active slot's CC number
Short Press (Control Button #5): Tapped BPM

Long Press (Slot Button): Assign the selected slot to an Envelope Follower or cycle which Envelope Follower is assigned.

Double Press (active slot):
Double Press (Control Button #0): Cycle an Envelope Follower’s filter type forward through the list (Linear, Opposite, Exponential, Random).
Double Press (Control Button #1): Cycle an Envelope Follower’s filter type backward through the list (Random, Exponential, Opposite, Linear).
Double Press (Control Button #4): Undo unsaved changes (reset EEPROM)
Double Press (Control Button #5): Save configuration

Multi-Button: 
Pressing Button #0 and Button #1 simultaneously will cycle the Envelope Follower’s ARG method (PLUS, MIN, etc.) if ARG.
Pressing Button #2 and Button #3 will cycle light modes
Pressing Button #4 and Button #5 will Turn EF on [if not already on] and randomly assign envelope.


Note: The actual mapping of these press types to your buttons can differ depending on how you coded handleSingleButtonPress(), onLongPress(), etc. in ButtonManager. Check your firmware’s ButtonManager.cpp for exact references.

3.3 OLED Display
Shows the active slot number and the pot’s assigned CC/channel.
Briefly displays messages like “EF ON,” “Slot → EF 2,” or “Saved!” after certain button actions.
May present advanced info: envelope levels, beat position, or other system statuses.

3.4 LED Indicators
Red: Often used to mark the active slot (the one the pot is currently controlling).
Green: Envelope mode enabled (global or per-slot).
Blue: Some modes might use blue for special states.
All LED colors and brightness are customizable in code or via configuration.

4. Advanced Features
4.1 Envelope Follower
Standard Envelope Follower (SEF): Tracks input amplitude (e.g., from an audio pin) and maps it to 0–127.
ARG Mode: Combines two audio signals (A & B) using plus/minus/exponential functions.
You can assign any of the 42 slots to be modulated by an Envelope Follower. When assigned, the slot’s MIDI output reflects potValue ± envelopeValue (or other combos).
Filter Types: Lowpass, Highpass, Bandpass, or special curves (Opposite, Exponential, etc.).
Button-based: Typically, you hold a button to cycle the assignment or filter type, or to toggle the Envelope Follower’s active state.

4.2 Serial Commands & Bulk Updates
Over a USB serial terminal (e.g., Arduino Serial Monitor), you can type:

SET_POT <slotIndex>,<channel>,<ccNumber> – Assign a specific slot to a channel/CC.
GET_ALL – Print all slot settings.
SET_ALL ... – Bulk update multiple slots in a single command.
SAVE – Immediately store the current configuration in EEPROM.

4.3 EEPROM Storage
All slot mappings, Envelope Follower settings, and LED preferences are retained on power-down.
A “factory reset” button combo or command can wipe the EEPROM back to defaults (Channel 1, sequential CCs, etc.).

5. Troubleshooting
Symptom	Potential Causes        /       Solutions
Device not recognized	        - Check USB cable/port.
                                - Restart or re-scan in your DAW.
No MIDI output	                - Verify the active slot’s channel/CC matches your synth’s channel.
                                - Double-check pot assignment.
Envelope not responding	        - Make sure Envelope Follow is globally ON.
                                - Confirm audio is fed into the correct analog pins.
LEDs not updating	            - Verify LED brightness in code.
                                - Ensure ledManager.update() is called regularly and your dirtyFlags are cleared properly.
Buttons unresponsive	        - Check wiring (if direct) or multiplexer (if virtual).
                                - Verify state machine timing (debounce, long-press delay, etc.).
EEPROM changes not persisting	- Confirm you have saved changes (e.g., SAVE command or dedicated save button).
                                - Check if EEPROM writes are disabled or broken.

6. Future Updates & Customization
HTML Firmware Editor: A web-based GUI tool that lets you reassign CC slots, LED colors, and Envelope Follower details visually. Available very soon.

7. Technical Specifications
MCU: Teensy 4.1 (or similar)
MIDI Protocol: USB + Serial (DIN)
Potentiometers: Now replaced by 1 physical pot + 42 “virtual” pot slots in firmware
Buttons: Multiple momentary, read via direct pins or multiplexer
LEDs: WS2812 addressable
Display: SSD1306 OLED (128×64) – originally described as 3×20 in older docs, but actual usage may vary
Power: 5V via USB or external supply
EEPROM: Onboard, used to store settings

8. Contact & Support
For firmware updates, detailed documentation, or to file issues, see our GitHub repository or contact support@bseverns.me.
We welcome suggestions for expansions or modifications to match your performance style.

Enjoy the new, streamlined MOARkNOBZ experience! With just one physical pot, you have the power of 42 virtual CC slots at your fingertips— wild modulation opportunities from Envelope Followers, LED feedback, and more. Happy music-making!