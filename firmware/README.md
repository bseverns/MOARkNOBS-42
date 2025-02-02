# The MOARkNOBZ - 42
BS - MOARkNOBS MN42 MIDI Controller User Manual

Introduction

The MOARkNOBS-42 is a feature-rich MIDI controller designed for dynamic performance and parameter modulation. Equipped with potentiometers, buttons, an OLED display, and LED indicators, it provides real-time MIDI control with envelope following and customizable settings.

1. Features Overview

-42 potentiometers for MIDI control

-6 momentary buttons with multiple functions

-Envelope Follower for dynamic parameter modulation

-OLED display for visual feedback

-Addressable LEDs for mode and status indication

-MIDI over USB and hardware serial

-EEPROM storage for saving settings

-Configurable via USB serial interface

2. Getting Started

2.1 Unboxing and Setup

Connect the MN42 controller to your computer via USB.

If using external power, ensure a stable 5V 2A power source.

Install your required MIDI software/hardware (DAW, synths, etc.).

Ensure your system recognizes the device as "MOARkNOBZ".

2.2 Powering On

Upon power-up, the OLED displays "MOAR" and initializes settings.

LEDs indicate envelope mode (green), active pot (red), and MIDI mode.

2.3 Configuring MIDI Channels and CCs

Default EEPROM settings:

All pots mapped to Channel 1

CC numbers assigned sequentially (0–41)

Envelope mapping defaults stored in EEPROM

Use the Benz EEPROM Settings Manager (HTML interface) or serial commands to reconfigure.

3. Using the Controls

3.1 Potentiometers

Adjust MIDI CC values in real-time.

Each pot’s assigned CC and channel can be customized via user settings or the HTML firmware editor.

LED feedback reflects the current control value and status.

3.2 Buttons

Single Press Functions:

B0 - Toggle Envelope Follower

B1 - Assign active pot to envelope

B2 - Cycle through MIDI channels (1–16)

B3 - Save configuration to EEPROM

B4 - Randomize all MIDI CC assignments

B5 - Factory reset (confirmation required)

Double Press Functions:

B0: Cycle envelope filter modes (Linear, Opposite, Exponential, Random)

B1: Swap envelope assignments for the active pot

3.3 OLED Display

-Displays active pot, channel, and mode.

-Provides real-time envelope visualization.

-Status messages for button actions.

3.4 LED Indicators

Red: Active potentiometer

Blue: Mode selection

Green: Envelope mode active

Brightness and color customizable via firmware editor.

4. Advanced Features

4.1 Envelope Follower

Converts audio amplitude to MIDI CC modulation with configurable filter types:

Linear: Direct mapping

Opposite Linear: Inverted response

Exponential: Nonlinear mapping for subtle changes

Random: Introduces randomness to the modulation

Assign pots to envelopes using Button B1.

4.2 Serial Commands

SET_POT,x,y,z → Assign Pot x to Channel y, CC z

GET_ALL → Retrieve all pot assignments

SET_ALL ... → Bulk update settings

SAVE → Store current settings in EEPROM

4.3 EEPROM Storage

MIDI mappings, envelope settings, and LED configurations persist after power cycles.
Reset EEPROM with Button B5.

5. Troubleshooting

Issue - Device not recognized

Solution - Check USB cable and port. Restart your DAW.

Issue - No MIDI output

Solution - Verify MIDI channels and CC numbers. Test with another software.

Issue - Envelope not responding

Solution - Ensure Envelope Follower is active and assigned correctly.

Issue - LEDs not updating

Solution - Check LED brightness setting and dirty flag updates.

Issue - Buttons unresponsive

Solution - Check debounce logic and ensure proper grounding.

6. Future Updates & Customization

Firmware updates may introduce additional MIDI modes and features.

Open-source architecture allows user modification.

External sensors can be integrated via available analog inputs.

7. Technical Specifications

MCU: Teensy 4.1

MIDI Protocol: USB & Serial

Potentiometers: 42, multiplexed

Buttons: 6, multiplexed

LEDs: WS2812 (Addressable)

Display: SSD1306 OLED (128x64)

Power: USB 5V

8. Contact & Support

For firmware updates and documentation, visit: the MN42 GitHub Repository or email support@bseverns.me.

Enjoy your MOARkNOBZ MIDI Controller!