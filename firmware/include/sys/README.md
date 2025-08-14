# System Report

`systemReportJSON()` spits a tiny JSON brag sheet.
It pulls `FW_VERSION` and `GIT_SHA` from the build and
packs them like `{ "fw": "1.2.3", "git": "deadbeef" }`.

Use it when you want the firmware to shout its identity
before someone asks "what's on this board?".
