# OSC Bridge

The rig spits OSC over UDP like a riot grrrl fanzine. These patches let you catch the stream and even throw a few punches back.

## Try this with Max

1. Fire up Max and open [`examples/max/mn42_listener.maxpat`](examples/max/mn42_listener.maxpat).
2. Watch the `print slot` window spew slot data coming in on port **9000**.
3. Bang the message box to hurl `/slot/0/value 0.5` back at the device on **9001**.
4. Wire in a MIDI knob if you're feeling extra—they pass straight through `ctlin` → `ctlout`.

## Or do it with Pure Data

1. Launch [`examples/puredata/mn42_listener.pd`](examples/puredata/mn42_listener.pd).
2. Slot chatter from port **9000** pipes into the console.
3. Click the message for a quick parameter tweak, or hijack the `ctlin`/`ctlout` pair for MIDI mischief.

These are just starting points. Hack, remix, and make the bridge scream your tune.
