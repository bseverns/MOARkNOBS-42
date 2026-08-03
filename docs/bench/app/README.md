# App Bench Receipts

This folder holds App-specific bench receipts used as operator-facing evidence.

This is an evidence folder. For tie-break rules, see [Documentation Truth Map](../../reference/DocumentationTruthMap.md).

## What belongs here

- App-over-Bridge-session bench receipts
- direct WebSerial bench receipts
- narrowly scoped operator notes tied to an executed App HIL run

## Current references

- [2026-05-31 live App-over-Bridge-session receipt](2026-05-31-app-over-bridge-session-summary.md)
- [2026-08-03 App Bridge-session simulator surface capture](2026-08-03-app-bridge-session-simulator-surface.md)
- [App-over-Bridge-session template](TEMPLATE_app-over-bridge-session-summary.md)
- [Direct WebSerial template](TEMPLATE_direct-webserial-summary.md)
- [Historical bridge-session supporting receipt](../firmware/0526_bridge-session-summary.md)
- [Historical boot-contract supporting receipt](../firmware/0526_boot-contract-summary.md)

## Receipt rules

- Fill `Date/time`, `Git commit/tag`, `Host OS`, `Browser`, `Node version`, `Firmware env flashed`, `Firmware git_sha`, `Schema version`, `Board revision`, `Board rework state`, `Serial port`, `Transport mode reported by App`, and `Artifact paths if any`.
- If a field was not captured, write `unknown/not captured`.
- Record the exact operator actions used to produce the result.
- State whether bridge raw fallback was used. If it was used, document that explicitly instead of implying structured App-over-bridge proof.
- Link supporting bridge or firmware receipts when they are prerequisites, but do not use them as substitutes for App proof.
- Do not expand browser, hardware, or release-readiness claims beyond what the receipt actually proves.
