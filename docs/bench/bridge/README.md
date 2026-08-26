# Bridge Bench Receipts

This folder holds bridge-specific bench receipts used as release and beta evidence.

This is an evidence folder. For tie-break rules, see [Documentation Truth Map](../../reference/DocumentationTruthMap.md).

## What belongs here

- structured bridge-session HIL receipts
- packaged console/server HIL receipts
- bridge-specific operator notes that stay narrowly tied to an executed bench run

## Current references

- [2026-08-26 structured Bridge-session failure receipt](2026-08-26-structured-bridge-session-failure.md)
- [2026-05-30 structured bridge-session HIL receipt](2026-05-30-structured-bridge-session-summary.md)
- [Historical structured bridge-session receipt](../firmware/0526_bridge-session-summary.md)
- [Structured bridge-session template](TEMPLATE_bridge-session-summary.md)
- [Packaged console HIL template](TEMPLATE_packaged-console-hil-summary.md)

## Receipt rules

- Fill `Date`, `Commit`, `Node`, `Bridge mode`, `Board revision`, `Board rework state`, `Firmware env`, and `Artifact / report path`.
- If a field was not captured, write `unknown/not captured`.
- Preserve the exact runner `PASS` output in a fenced `text` block.
- Do not expand support claims beyond what the receipt actually proves.
