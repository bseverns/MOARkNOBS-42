# Bridge Release Artifacts Checklist

Use this template for each release tag (for example `v1.0.0`).

## Release metadata

- Release tag: `vX.Y.Z`
- Date (UTC): `YYYY-MM-DD`
- Release manager: `name`
- Bridge version source: `bridge/package.json` + tag mapping confirmed

## Preflight

- [ ] `npm --prefix bridge ci`
- [ ] `npm --prefix bridge test`
- [ ] `npm --prefix bridge run smoke`
- [ ] CLI docs match shipping behavior: `bridge/README.md`
- [ ] Quickstart docs match shipping behavior: `docs/OSCBridge.md`
- [ ] Performer sheet updated if needed: `docs/BridgeForPerformers.md`
- [ ] `.github/workflows/release.yml` bridge package matrix passed for all targets

## Packaging outputs

### macOS x64

- [ ] Build artifact produced
- [ ] Launches successfully
- [ ] Opens serial connection to MN42
- [ ] Creates/advertises MIDI device `MN42 Bridge`
- [ ] OSC `/mn42/slots` stream verified
- [ ] OSC `/mn42/cmd` `SET_SLOT_VALUE` round-trip verified
- [ ] Signed/notarized (if release policy requires)
- Artifact name:
- SHA256:

### macOS arm64

- [ ] Build artifact produced
- [ ] Launches successfully
- [ ] Opens serial connection to MN42
- [ ] Creates/advertises MIDI device `MN42 Bridge`
- [ ] OSC `/mn42/slots` stream verified
- [ ] OSC `/mn42/cmd` `SET_SLOT_VALUE` round-trip verified
- [ ] Signed/notarized (if release policy requires)
- Artifact name:
- SHA256:

### Windows x64

- [ ] Build artifact produced
- [ ] Launches successfully
- [ ] Opens serial connection to MN42
- [ ] Creates/advertises MIDI device `MN42 Bridge`
- [ ] OSC `/mn42/slots` stream verified
- [ ] OSC `/mn42/cmd` `SET_SLOT_VALUE` round-trip verified
- [ ] Signed (if release policy requires)
- Artifact name:
- SHA256:

### Linux x64

- [ ] Build artifact produced
- [ ] Launches successfully
- [ ] Opens serial connection to MN42
- [ ] Creates/advertises MIDI device `MN42 Bridge`
- [ ] OSC `/mn42/slots` stream verified
- [ ] OSC `/mn42/cmd` `SET_SLOT_VALUE` round-trip verified
- Artifact name:
- SHA256:

## Release attachments

- [ ] Bridge artifacts uploaded to GitHub release
- [ ] `THIRD_PARTY_LICENSES.md` attached or linked
- [ ] `bridge/THIRD_PARTY_LICENSES.json` attached or linked
- [ ] Checksums file uploaded

## Sign-off

- [ ] Firmware + bridge artifact set validated together
- [ ] Notes posted in release description
- [ ] Team approval complete

Comments:
