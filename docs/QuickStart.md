# QuickStart: Wire, Flash, and Make It Blink

Welcome to the five-minute joyride that gets your MOARkNOBS-42 from raw board to loud, flashing mischief. This guide is for the fresh-outta-the-box crowd: no ego, no gatekeeping, just practical steps to boot the beast.

## Wire It Up

Start with the bare essentials. We'll keep the spaghetti minimal:

1. **Power** – Feed the Teensy 5 V through `VUSB` and ground everything like your sanity depends on it.
2. **LED Data** – Run Teensy pin `6` through a ~330 Ω resistor into the first WS2812's `DIN`. Chain the rest like dominoes.
3. **Buttons/Encoders** – Follow the pin labels on the board. Short wires = less noise.

Need visuals? Scope these schematics:

![Interface & LED wiring](sketch/PNG_MOAR_Schematic/SCH_MOAR_Schematic_1-INTERFACE-LED-MIDI-CNTRL.png)

![Power & button test rig](sketch/PNG_MOAR_Schematic/SCH_MOAR_Schematic_3-PWR-BUTTON-TEST.png)

## Flash the Brain

1. **Install PlatformIO** – `pip install platformio` or use the VS Code add-on.
2. **Plug in the Teensy 4.0** over USB.
3. **Build and upload** the main firmware from the `firmware/` directory:
   ```bash
   pio run -t upload -e teensy40_main
   ```
   The loader will yell success when it's done.

## First-Run Tests

Once the firmware's on board, make sure the basics don't flake out:

1. **Blink test** – Drop this mini sketch in `firmware/test/` and upload it the same way:
   ```cpp
   #include <FastLED.h>
   #define LED_PIN 6
   #define NUM_LEDS 1
   CRGB leds[NUM_LEDS];
   void setup(){ FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS); }
   void loop(){
     leds[0]=CRGB::Green; FastLED.show(); delay(500);
     leds[0]=CRGB::Black; FastLED.show(); delay(500);
   }
   ```
   If that lone pixel blinks, you're in business.
2. **Button sanity** – Build the hardware test suite:
   ```bash
   pio run -e teensy40_full_system
   ```
   Follow the serial prompts to poke every switch and LED.

## Troubleshooting Like a Rebel

- **LEDs dead?** Double-check ground and that pin `6` isn't doing a sulk.
- **Upload hangs?** Mash the Teensy's button to wake the bootloader.
- **Random resets?** Your 5 V rail is sagging—use a beefier supply.
- **Nothing works?** Walk away, breathe, then come back with a multimeter.

You're now dangerous. For deeper dives, the rest of the docs are waiting.
