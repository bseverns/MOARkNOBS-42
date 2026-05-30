# App Bench Summary: Direct WebSerial

Date/time: unknown/not captured
Git commit/tag: unknown/not captured
Host OS: unknown/not captured
Browser: unknown/not captured
Node version: unknown/not captured
App URL: unknown/not captured
Origin security: localhost or HTTPS or unknown/not captured
Firmware env flashed: unknown/not captured
Firmware git_sha: unknown/not captured
Schema version: unknown/not captured
Board revision: unknown/not captured
Board rework state: unknown/not captured
Serial port: unknown/not captured
Transport mode reported by App: unknown/not captured
Artifact paths: unknown/not captured

## Exact test actions

1. Serve the App from `localhost` or HTTPS and record the URL.
2. Connect over direct WebSerial and record the App transport mode.
3. Record `HELLO`, manifest, schema, and config hydrate evidence.
4. Stage one safe config mutation and record the exact field changed.
5. Apply the staged change through the direct WebSerial path and record the `SET_ALL`/ACK evidence.
6. Confirm readback shows the mutation.
7. Confirm cleanup restores the original config.

## Result

PASS or FAIL

## Proven

- App was served from `localhost` or HTTPS.
- Direct WebSerial connect succeeded.
- `HELLO`, manifest, schema, and config hydrate succeeded.
- One safe staged edit applied through `SET_ALL`.
- Readback confirmed the mutation.
- Cleanup restored the original config.

## Caveats

- Note any browser support, host setup, board state, or evidence limitations here.
- Do not imply broader browser support than the receipt actually proves.

## Supporting evidence

- Supporting firmware receipt: unknown/not captured
- Supporting bridge receipt: not applicable or unknown/not captured

## Artifact notes

- Browser screenshots, Playwright traces, console logs, serial captures, or readback dumps: unknown/not captured
