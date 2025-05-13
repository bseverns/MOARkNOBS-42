#MOARkNOBS-42
MIDI controller with 42 (CC) virtual potentiometers, 6 envelope followers, chaotic interactions.

## Overview

The MOARkNOBS-42 (MN42) Controller is a versatile MIDI and envelope-follower-based hardware interface designed to provide musicians and artists with expressive real-time control of audio parameters. Featuring 42 control slots, RGB LED feedback, OLED display integration, and robust envelope-following capabilities, MN42 is ideal for live performance, studio production, and interactive installations.

## Features

* **42 Button Slots**: Each button slot is individually assignable to any MIDI Control Change (CC) message.
* **Envelope Followers**: Supports both Standard Envelope Following (SEF) and Advanced Relative Generation (ARG) modes.
* **Multiplexed Button Interface**: Offers intuitive short, long, and double press actions for dynamic parameter control. See below for manual.
* **OLED Display**: Provides real-time feedback on MIDI parameters, envelope levels, modes, and status messages.
* **RGB LEDs**: Instant visual feedback through dynamic LED indicators, clearly reflecting active pots, envelope states, and MIDI interactions.
* **Robust MIDI Integration**: Supports both serial MIDI and USB MIDI connections with reliable real-time messaging.
* **Web-Based Configuration**: Easily configure EEPROM settings and system parameters through a user-friendly web interface.
* **Priority Task Scheduling**: Real-time optimized task management ensuring stable performance under demanding conditions.

## Hardware Specifications

* **Microcontroller**: Teensy 4.0
* **Analog Inputs**: 42 multiplexed potentiometers
* **Digital Inputs**: 6 direct control buttons
* **LED Feedback**: WS2812 RGB LED strips (42 LEDs)
* **Display**: 128x64 OLED (SSD1306)
* **Connection**: MIDI DIN (Serial) and USB MIDI
* **Power Supply**: USB 5V or external regulated 5V supply

## Using the Controller

### Initial Setup

1. **Upload Firmware**: Use PlatformIO or Arduino IDE with Teensyduino to upload the provided firmware to the Teensy 4.0.
2. **Hardware Assembly**: To be expanded
3. **Connections**: To be expanded

### Configuration

* **Web Interface**: Open the provided `website-to-be-named` in a web browser supporting WebSerial to configure MIDI channels, CC numbers, envelope assignments, and LED settings.
* **OLED Feedback**: Real-time parameter values and statuses are continuously displayed on the OLED screen during operation and can be saved with a double-tap of Control #5.

### Performance Operation

* **Envelope Modes**: Toggle envelope followers on/off and select modes (SEF/ARG) dynamically during use.
* **Button Interaction**:

* Short Press:
* Slot Buttons (0-41): Select active potentiometer.

* Control Button #0: Toggle Envelope Follower (EF) mode ON/OFF.

* Control Button #1: Select next slot.

* Control Button #2: Cycle assigned Envelope Follower.

* Control Button #3: Cycle active slot MIDI channel (1-16).

* Control Button #4: Cycle active slot MIDI CC number.

* Control Button #5: Tap tempo for internal BPM.

* Long Press:
* Slot Buttons (0-41): Dynamically assign or cycle envelope followers.

* Double Press:
* Slot Buttons (0-41): Cycle envelope follower filter types.

* Control Button #0: Cycle envelope filter list forward.

* Control Button #1: Cycle envelope filter list backward.

* Control Button #3: Reset EEPROM to previously saved settings. //to be reset in firmware

* Control Button #4: Save current configuration to EEPROM.

* Button Combination Short Press:
* Control #0 + Control #1: Cycle ARG method if ARG mode active.

* Control #2 + Control #3: Cycle LED modes.

* Control #4 + Control #5: Activate EF mode and assign a random envelope follower

## Maintenance

* Regularly back up your EEPROM settings via the web interface.
* Ensure firmware updates match hardware revisions and PCB layouts.

## Troubleshooting

* **No MIDI Output**: Verify MIDI channel and CC assignments in EEPROM settings.
* **LED/OLED Issues**: Confirm correct wiring per PCB Gerber file and test power supply stability.
* **Envelope Instability**: Adjust smoothing/filter parameters within the web interface or firmware.

## License

MOARkNOBS Controller firmware and hardware design files are provided under the MIT License. You are free to modify and redistribute with attribution.

## Author

Designed and developed by the BSSS project team.

---

For detailed setup instructions, hardware schematics, and latest updates, refer to the project repository.
