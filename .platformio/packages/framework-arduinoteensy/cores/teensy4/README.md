# Teensy Core Patch

This directory hijacks Teensy's own core so we can fix a nit in `dtostrf`.
The upstream code compares signed and unsigned integers like it's no big deal.
We cast the precision counter so the compiler chills out.

Use this as a teaching moment: always line up your types before the
compiler throws a tantrum.

## sm_malloc_stats schooling
We also rewired the small-malloc stats tracker so its counters use `size_t`.
If the legacy API still hands over an `int`, we smack a cast on our side and keep riding.
Read the source and learn why mixing signed and unsigned is a one-way ticket to Weirdsville.
