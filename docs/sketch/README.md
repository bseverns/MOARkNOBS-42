# Sketch Rack

> Board guts, trace glam, and subsystem scribbles for the moment the pretty block diagram stops answering questions.

Start here if you want to teach the hardware without making everyone boot a CAD tool first.

## Fast lap

1. **Whole-board view** – [`MOAR_BOARD.png`](MOAR_BOARD.png) shows the populated board from orbit.
2. **Copper chase** – [`TopLayer.png`](TopLayer.png) and [`BottomLayer.png`](BottomLayer.png) are the quick trace maps.
3. **Schematic cuts** – `ButtonMatrix.png`, `Interface&Cntrl.png`, `Power-Reg.png`, `MIDI.png`, `EFpair.png`, and `MainLEDPool.png` break the monster into smaller gremlins.
4. **Subsystem walkthroughs** – [`systemFlow/README.md`](systemFlow/README.md) is the guided tour with words, not just screenshots.

## Teaching move

Pick one signal and follow it twice: once in the image, once in [`systemFlow/hw/README.md`](systemFlow/hw/README.md). If the two stories disagree, the docs are stale and the board wins.
