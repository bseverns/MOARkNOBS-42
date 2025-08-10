# Example Patches

These patches are the training wheels and demolition derby for the controller.
Each one shows how to jack into the board through the OSC/WebMIDI bridge and
spy on the slots or sling MIDI back at it.

## Boot the Bridge

1. Plug the controller in over USB.
2. From the repo root run:
   ```bash
   node bridge/mn42_bridge.js
   ```
   The bridge spits OSC on `127.0.0.1:8000` and chats WebMIDI to the hardware.
3. When the Node shim is humming, open one of the patches below.

If the bridge is foreign territory, hit the [OSC Bridge docs](../OSCBridge.md)
for the gritty details.

## Pick Your Poison

- [max/](max/) – a Max patch that listens for slot and envelope traffic.
- [puredata/](puredata/) – the same idea in Pd for the open-source diehards.

These examples don't try to be synths; they're reference rigs. Use them to
validate a MIDI route, map out the OSC namespace, or just watch the envelopes
writhe. Once you're comfortable, shred them and build your own controller.
