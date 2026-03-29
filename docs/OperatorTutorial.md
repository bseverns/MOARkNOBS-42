# Operator Tutorial

This page is the practical answer to “how do I actually operate this thing without guessing?”

The browser configurator is not just a settings page. It is the live operating surface for:

- understanding what the board is currently doing
- staging edits without lying to yourself
- deciding when to apply those edits to the device
- managing profiles and backups
- protecting live controls from sudden jumps

TODO: add one annotated screenshot of the full configurator layout with callouts for `Live Slots`, `Selected Slot`, `Apply/Rollback`, `Recovery & Profiles`, and the utility rail.

## The three states you need to keep separate

If you keep these separate, the machine feels calm. If you mix them together, it feels haunted.

### 1. Live device state

This is what the firmware most recently confirmed.

- It is the truth the hardware is actually using.
- It updates after connect and after a successful apply.
- It is what profile save/load/reset operates on.

### 2. Staged browser state

This is what you are currently editing in the configurator.

- Most form edits change staged state first.
- The diff panel compares staged state to live device state.
- **Apply** is the moment staged state attempts to become live device state.

### 3. Browser-only operator state

This is local helper information that belongs to the operator, not the firmware.

- slot labels
- the MIDI badge
- the pickup guard / immediate response switch

These do **not** get written to the board.

That means:

- they do not require **Apply**
- they survive reconnects in the same browser
- they should not be mistaken for device memory

## First operating pass

Use this as the basic rehearsal flow.

1. Connect the board.
2. Confirm the header shows the expected device and firmware.
3. Click a slot in **Live Slots**.
4. Edit that slot in **Selected Slot**.
5. Watch the staged diff change.
6. Press **Apply** only when the staged change is intentional.
7. Save a profile only after the live device state is the state you want to keep.

## What the live slot switch means

Each slot tile has a tiny mode badge:

- `IM` = **Immediate local response**
- `PK` = **Browser-only pickup guard**

These are not sound engines or firmware modes. They are browser-side control behaviors for how the configurator responds when local control input becomes active.

## Immediate local response (`IM`)

Immediate mode means the configurator treats the slot as ready to respond right away.

Use it when:

- you are editing calmly at the bench
- the physical control position already matches what you expect
- you want the fastest local response with no guard behavior

What it means in practice:

- local control input is accepted immediately
- there is no “wait until the knob catches the stored value” safety step
- if the physical control and current slot value disagree, the result can jump quickly

This is fine when you want speed and you trust the current physical position.

## Browser-only pickup guard (`PK`)

Pickup mode adds a local safety gate.

The configurator waits until the physical control passes through the current effective value before it starts treating the control as active again.

Use it when:

- you are reconnecting in the middle of a session
- the physical control might not match the current stored/live value
- you want to avoid sudden jumps while rehearsing or performing

What it means in practice:

- the browser remembers that this slot should be guarded
- local response is delayed until the control “catches” the current value
- once it catches, adjustment proceeds normally

This is especially useful after reconnecting, switching profiles, loading a backup, or changing a slot in the app while the physical control has not moved yet.

## Why pickup guard is browser-only

The pickup guard is an operator convenience, not device configuration.

That is why it is browser-only:

- the firmware does not persist it
- another browser or computer will not know your local guard choices
- enabling it should not dirty the config or imply a hardware write

Think of it as part of your local cockpit, not part of the instrument’s saved patch.

## When to choose `IM` versus `PK`

Use `IM` when:

- you want direct response
- you are bench testing
- you just moved the relevant physical control and know it is in the right neighborhood

Use `PK` when:

- you are about to touch a control after reconnecting
- you are in rehearsal or performance mode
- you are worried about abrupt jumps more than raw speed

If you are unsure, default to `PK` for live work and `IM` for deliberate setup work.

TODO: add one close-up screenshot of the live slot tile showing `IM` and `PK` with a short caption explaining that this is browser-local behavior, not firmware storage.

## Editing safely

The safest mental model is:

1. select the slot
2. make the edit
3. inspect the diff
4. apply intentionally
5. save the profile only if that live result is worth keeping

This is safer than treating every field as a live hardware write.

## Profiles, backups, and recovery

There are two different kinds of “saving” in the configurator.

### Save profile

This stores the current live device state into an on-device profile slot.

Use it when the board itself should remember the state.

### Download / upload profile

This is file backup and restore through the configurator.

Use it when:

- you want an external copy
- you want to move a setup between machines
- you want a rollback point before experimental edits

### Reset profile

This restores the active profile to its baseline and pushes that result back through the normal profile path.

Treat it as destructive. Make a backup first if the current state matters.

## What to watch during operation

The configurator gives you three useful truth surfaces:

- the connection banner: which device you are actually talking to
- the staged diff: what has changed but is not yet applied
- the status/console area: whether the last action succeeded, failed, or rolled back

If these three surfaces agree, you are usually safe.

If they disagree, stop and resolve that before making more changes.

## Suggested rehearsal workflow

Before rehearsal:

1. connect and confirm identity
2. load the intended profile
3. enable `PK` on slots where sudden jumps would be risky
4. make any needed staging edits
5. apply
6. save the updated profile only if you want the board to keep it

During rehearsal:

1. keep an eye on the connection/status surface
2. use the slot labels and browser-local guards as operator aids
3. avoid profile reset unless you are intentionally restoring a baseline

After rehearsal:

1. export a backup if the session produced a good state
2. note any browser-local guard/label decisions you want to recreate elsewhere

## Read next

- [Configurator Tour](Configurator.md) for the transport and staged/live contract
- [Profile Workflow](ProfileWorkflow.md) for save/load/reset behavior
- [Failure-First Guide](FailureFirst.md) if you want to understand what happens when things go wrong
- [Testing](TESTING.md) for what the repo actually proves
