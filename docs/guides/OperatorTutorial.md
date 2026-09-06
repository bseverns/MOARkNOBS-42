# Operator Tutorial

This page is the practical answer to “how do I actually operate this thing without guessing?”

The browser configurator is not just a settings page. It is the live operating surface for:

- understanding what the board is currently doing
- staging edits without lying to yourself
- deciding when to apply those edits to the device
- managing profiles and backups

![Annotated configurator screenshot showing connection identity, the State drawer, and the selected-slot signal path.](../assets/ui/configurator-top-annotated.png)

![Annotated configurator screenshot showing live slots, the selected-slot signal path, Reactive shaping, and shared LFO Motion.](../assets/ui/configurator-workbench-annotated.png)

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
3. make any needed staging edits
4. apply
5. save the updated profile only if you want the board to keep it

During rehearsal:

1. keep an eye on the connection/status surface
2. use the slot labels and MIDI badges as operator aids
3. avoid profile reset unless you are intentionally restoring a baseline

After rehearsal:

1. export a backup if the session produced a good state
2. note any browser-local labels you want to recreate elsewhere

## Read next

- [Configurator Tour](Configurator.md) for the transport and staged/live contract
- [Profile Workflow](ProfileWorkflow.md) for save/load/reset behavior
- [Failure-First Guide](../validation/FailureFirst.md) if you want to understand what happens when things go wrong
- [Testing](../validation/TESTING.md) for what the repo actually proves
