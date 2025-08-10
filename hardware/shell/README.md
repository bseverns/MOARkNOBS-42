# MOARkNOBS Shell

Welcome to the plastic armor for the button board. This folder holds the CAD guts for printing and tweaking the enclosure so your knobs have something loud to live in.

## Directory Layout

- `3DShell_btnBRD/` – STEP source for the enclosure. `*_T.step` is the top half, `*_B.step` is the bottom. Crack these open in FreeCAD (or any STEP-savvy CAD) if you want to remix the design or check clearances before you commit plastic.
- `stl/` – Exported meshes ready for slicers. `*_T.stl` and `*_B.stl` mirror the STEP files but are triangulated for 3D printing.

## Viewing & Editing

1. Install [FreeCAD](https://www.freecad.org/) or another tool that speaks STEP.
2. Launch it, import the `.step` files from `3DShell_btnBRD/`, and poke around. All dimensions assume a 1.6 mm PCB riding on the standoffs.
3. Want to tweak? Duplicate the file and mod away. Keep the board keep-outs unless you love sanding.

## Printing

1. Drag the `.stl` files into your slicer of choice (PrusaSlicer, Cura, whatever keeps your printer honest).
2. Use a 0.2 mm layer height, 2–3 perimeters, and ~20% infill. PETG survives car trunks; PLA works if you stay out of the sun.
3. Orient the parts so the flat faces sit on the build plate. The button recesses face up.
4. Slice, print, and let the parts cool before yanking them off.

### Slicer Tips

- Enable support only if your printer hates bridges; the models are designed to print without it.
- Holes for M3 heat-set inserts are 3.8 mm; ream them with a soldering iron and an insert, not brute force.
- Add a brim if your bed adhesion is sketchy.

## Fit & Alignment Gotchas

- The standoffs expect a 1.6 mm board and line up with the button matrix. If your board is thicker or thinner, shim or file accordingly.
- Top and bottom halves mate with a 0.2 mm clearance. If your printer over-extrudes, sand the edges before forcing them together.
- Button caps sit close to the lid; any warp in the top shell will rub. Keep the top flat during cooling.
- Heat-set inserts sit flush; press them straight or the screws will skew the board.

## Why These Files Exist

The STEP files are the source of truth—edit those when evolving the enclosure. The STL files are disposable exports meant for printers. Keep this split so we can track parametric history without bloating the repo with mesh noise.

Have fun, break stuff, and post pics when you inevitably sharpie your own logo on it.

