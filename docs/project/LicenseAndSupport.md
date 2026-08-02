# License and Support

This page explains the repo's license split and support boundary in plain English. It does not replace the actual license texts.

## Software and firmware

The repository's software and firmware are covered by the MIT license in the repo-root `LICENSE` file.

Plain-English summary:

- You can use, modify, share, and sell software derived from it.
- You must keep the copyright and license notice with substantial copies.
- It is provided without warranty.

## Hardware design files

The hardware documentation/design files are covered by CERN OHL v2 Strongly Reciprocal in `hardware/LICENSE`.

Plain-English summary:

- You can study, modify, fabricate, and sell hardware made from the design files.
- If you distribute modified hardware documentation, you must keep it under the same hardware license.
- Attribution/license notices need to stay with the documentation and, where practical, with products derived from it.
- It is provided without warranty.

## If you remix, fabricate, or sell derivatives

- Software-only changes follow the MIT rules in the repo root license.
- Hardware-documentation changes follow the CERN OHL rules in `hardware/LICENSE`.
- If your work combines both, treat the software and hardware layers separately and keep both license obligations intact.
- If you sell derivative hardware, do not remove attribution/license notices that are required by the hardware license.

## What this repo does and does not promise

What the repo does:

- publishes source, documentation, and build/test guidance
- documents the current known workflows and some failure modes
- makes remixing and learning possible

What the repo does not promise:

- warranty of fitness, safety, merchantability, or support response times
- guaranteed sourcing availability
- guaranteed compatibility for undocumented substitute parts
- guaranteed success from stale or unpublished fabrication assets

## Practical support boundary

- Use the docs as the first support layer.
- Treat hardware substitutions, fabrication package status, and real-board bring-up as things that may still require bench validation.
- If a file is not present in the repo, do not assume an older doc reference still describes the current recommended build.

## Ask For Help Or Report A Problem

- Use [GitHub Issues](https://github.com/bseverns/MOARkNOBS-42/issues) for public questions and reproducible bugs.
- Include firmware version, operating system/browser, App or Bridge path, exact error, and whether physical hardware is involved.
- Follow the [security policy](https://github.com/bseverns/MOARkNOBS-42/blob/main/SECURITY.md) for private vulnerability reports.

Do not post credentials, private device data, or other secrets in a public issue. These contact paths do not promise a response time.
