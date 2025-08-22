#pragma once

// The MIDI library can't keep its story straight on a few meta-event names.
// These aliases paper over the churn so our code builds no matter which
// flavor of the library you're riding.
// If upstream defines its own MidiType_* macros, we step aside.
#ifndef MidiType_SystemExclusiveStart
#define MidiType_SystemExclusiveStart midi::SystemExclusive
#endif
#ifndef MidiType_SystemExclusiveEnd
#define MidiType_SystemExclusiveEnd midi::EndOfExclusive
#endif
#ifndef MidiType_Tick
#define MidiType_Tick midi::Clock
#endif
