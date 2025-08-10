# Teensy Core Patch

This directory hijacks Teensy's own core so we can fix a nit in `dtostrf`.
The upstream code compares signed and unsigned integers like it's no big deal.
We cast the precision counter so the compiler chills out.

Use this as a teaching moment: always line up your types before the
compiler throws a tantrum.
