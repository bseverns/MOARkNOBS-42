# BTN_42 Hardware

This directory contains the button-board design used by the MOARkNOBS controller.

## MN42-1

The `MN42-1` folder is the first PCB revision. It provides everything needed to build the board:

- **BOM_btnBRD_btnBRD_2025-04-17.xlsx** – complete bill of materials.
- **PickAndPlace_btnBRD_2025-04-17.xlsx** – reference positions for automated assembly.
- **Gerber_btnBRD_2025-04-17.zip** – Gerber package for fabrication.
- **shell/** – STEP and STL models of the enclosure. `3DShell_btnBRD/` holds the STEP files, while `stl/` contains printable STL meshes.

Solder the components according to the BOM and orientation in the pick-and-place sheet. Once populated, mount the board inside the 3D printed shell.

## License

All hardware files are released under the MIT License, matching the rest of this repository. See the [LICENSE](../../LICENSE) file for details.
