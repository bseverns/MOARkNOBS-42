# Release Guide

Need to cut a proper drop? Start with the quick steps in [Publishing a Release](../README.md#publishing-a-release), then walk through the gory details below.

1. **Run the gauntlet** – build (`pio -d firmware run -e teensy40_main`) and test (`pio -d firmware test -e teensy40_unity -vvv`, `npm --prefix bridge test`). Don't ship broken noise.
2. **Bump the version constants** – `firmware/include/Globals.h` holds `FIRMWARE_VERSION`. Update it and any other hard‑coded version strings.
3. **Update the changelog** – toss your highlights into `CHANGELOG.md` so future you remembers what changed.
4. **Commit it** – lock in the version bump and changelog with a commit.
5. **Tag it loud** – `git tag -a vX.Y.Z -m "vX.Y.Z"` to mark the moment.
6. **Push the tag** – `git push origin vX.Y.Z` kicks CI into gear. It builds the `.hex` and `sysreport.json`.
7. **Draft the release** – on GitHub, create a new release from that tag, drop any human‑written notes, and publish.
   - Attach `THIRD_PARTY_LICENSES.md` and the `firmware/LICENSES/` folder as release assets so the suits stay off our backs.
8. **Celebrate or debug** – if CI faceplants, fix it and retag. If it works, cue the lights.
