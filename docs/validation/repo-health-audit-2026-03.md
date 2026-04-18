# Repo Health Audit - 2026-03

Date refreshed: 2026-03-28

This file is now a pre-production orientation lock for the repo. It records what is already coherent, what is still intentionally unfinished, and what should not be overstated before a broader release.

## Scope

This pass looks at release clarity, firmware/app/bridge contract truth, hardware-documentation sync, support readiness, and first-time user orientation.

## Current repo state

### What is now coherent

- The browser configurator, bridge, and firmware agree on the production transport story.
- Browser profile save/load/reset is now a real firmware-backed path rather than simulator-only UI.
- Macro snapshot and scene storage are now real EEPROM-backed firmware features.
- Builder and performer quickstarts exist and give a short path into the repo:
  - [Quickstart for Builders](../getting-started/QuickstartForBuilders.md)
  - [Quickstart for Performers](../getting-started/QuickstartForPerformers.md)
- The configurator-versus-bridge decision is now answered in one place:
  - [Connectivity Guide](../getting-started/ConnectivityGuide.md)
- The software-versus-hardware license/support split is now explained in plain English:
  - [License and Support](../project/LicenseAndSupport.md)
- The current hardware-status page exists and is the right front door for hardware artifact truth:
  - [Hardware Current Build](../reference/HardwareCurrentBuild.md)

### What is still intentionally incomplete

- There is still no versioned BOM export in the repo.
- There is still no versioned fabrication archive in the repo.
- The bridge is still a Node-launched desktop tool, not a signed one-click installer.
- Broad browser and DAW compatibility claims are now documented conservatively, but the underlying proof is still narrower than a mass-market product page would imply.

## Verified repo facts

- Hardware reference PDFs are present in `hardware/MN42-machineDrawings/`.
- The software/firmware license is MIT in the repo-root `LICENSE`.
- The hardware documentation/design license is CERN OHL v2 Strongly Reciprocal in `hardware/LICENSE`.
- The repo contains builder, performer, validation, troubleshooting, configurator, and bridge docs that now point at one another cleanly enough to support a pre-production orientation pass.

## Remaining pre-production friction points

1. Hardware ordering is not yet locked from the repo alone.
   Reason:
   There is no versioned BOM or fabrication zip in-tree yet, so a builder still needs to treat the hardware package as pre-production rather than order-ready.

2. The desktop bridge story is functional but not productized.
   Reason:
   The bridge works and is documented, but it still assumes a Node-capable operator rather than a signed installer flow.

3. Browser compatibility should still be described narrowly.
   Reason:
   The repo proves Chromium-based configurator behavior most strongly. Other browser paths should stay conservative unless bench evidence exists; see [Host Compatibility](../reference/HostCompatibility.md).

4. Release-ready language should remain gated.
   Reason:
   The docs now support demo readiness and pre-production orientation, but broader release language still depends on manufacturing-package truth, bench evidence, and explicit support boundaries.

## What changed since the earlier March audit

The earlier audit identified five main documentation gaps:

1. Add one canonical hardware status page.
2. Add short builder/performer quickstarts.
3. Add a plain-language connectivity guide.
4. Add a plain-language license/support boundary doc.
5. Repoint README surfaces to those pages and reduce stale "latest" wording.

Those gaps are now substantially addressed in the repo. The remaining work is not basic orientation anymore; it is pre-production packaging and evidence.

## Pre-production reading order

If someone new needs to understand this repo without guessing, point them here:

1. [Start Here](../getting-started/StartHere.md)
2. [Quickstart for Builders](../getting-started/QuickstartForBuilders.md) or [Quickstart for Performers](../getting-started/QuickstartForPerformers.md)
3. [Connectivity Guide](../getting-started/ConnectivityGuide.md)
4. [Validation Flow](ValidationFlow.md)
5. [Hardware Current Build](../reference/HardwareCurrentBuild.md)
6. [License and Support](../project/LicenseAndSupport.md)

## Current conclusion

The repo is no longer suffering from orientation drift at the software/instrument level. It now reads like a coherent pre-production system.

The remaining hard truth is narrower:

- hardware package artifacts are not order-locked in the tree yet
- broad release-ready language should stay conservative
- installer-grade distribution and broad host/browser claims should not be implied before bench evidence exists
