# App Bench Summary: App Over Bridge Session

Date/time: unknown/not captured
Git commit/tag: unknown/not captured
Host OS: unknown/not captured
Browser: unknown/not captured
Node version: unknown/not captured
Bridge mode: source or packaged
App URL: unknown/not captured
Bridge console URL: unknown/not captured
Firmware env flashed: unknown/not captured
Firmware git_sha: unknown/not captured
Schema version: unknown/not captured
Board revision: unknown/not captured
Board rework state: unknown/not captured
Serial port: unknown/not captured
Transport mode reported by App: unknown/not captured
Raw fallback used: no/yes - explain
Artifact paths: unknown/not captured

## Exact test actions

1. Start the bridge console and record the URL.
2. Open `/app/` from the bridge-served origin.
3. Connect the App and record the transport mode shown by the App.
4. Record the manifest/schema/config hydrate evidence.
5. Stage one safe config mutation and record the exact field changed.
6. Apply the staged change and record checksum/ACK evidence.
7. Confirm cleanup restored the original config.

## Result

PASS or FAIL

## Proven

- Bridge console starts.
- `/app/` opens from the bridge-served origin.
- App reports Bridge session mode.
- Manifest, schema, and config hydrate succeeds.
- One safe config field is staged.
- Apply succeeds with checksum/ACK.
- Cleanup restores the original config.
- Raw fallback was not used unless documented below.

## Caveats

- Note any host, browser, Node, board, bridge packaging, or evidence limitations here.
- If raw `/ws` fallback was used, say why and do not treat this receipt as Bridge-session proof.

## Supporting evidence

- Supporting bridge receipt: unknown/not captured
- Supporting firmware receipt: unknown/not captured

## Artifact notes

- Browser screenshots, Playwright traces, HAR files, console logs, or bridge logs: unknown/not captured
