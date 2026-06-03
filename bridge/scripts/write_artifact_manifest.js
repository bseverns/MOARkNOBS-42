#!/usr/bin/env node

const fs = require('node:fs');
const path = require('node:path');

function getArg(name) {
  const index = process.argv.indexOf(name);
  if (index === -1 || index + 1 >= process.argv.length) return '';
  return String(process.argv[index + 1] || '').trim();
}

function requiredArg(name) {
  const value = getArg(name);
  if (!value) {
    throw new Error(`missing required argument: ${name}`);
  }
  return value;
}

function defaultSigningNote(signingStatus) {
  if (signingStatus === 'unsigned-ci-artifact') {
    return 'Unsigned CI artifact; not a signed public installer.';
  }
  if (signingStatus === 'signed') {
    return 'Artifact reports a signed status; verify signature evidence before public release.';
  }
  return 'Signing status was provided by the packaging lane.';
}

function main() {
  const stageDir = requiredArg('--stage-dir');
  const target = requiredArg('--target');
  const releaseTag = requiredArg('--release-tag');
  const commitSha = requiredArg('--commit-sha');
  const nodeTarget = getArg('--node-target') || target;
  const signingStatus = getArg('--signing-status') || 'unsigned-ci-artifact';
  const signingIdentity = getArg('--signing-identity') || null;
  const notarizationStatus =
    getArg('--notarization-status') ||
    (signingStatus === 'unsigned-ci-artifact' ? 'not-requested' : 'unknown');
  const signingNote =
    getArg('--signing-note') || defaultSigningNote(signingStatus);

  const entries = fs
    .readdirSync(stageDir, { withFileTypes: true })
    .filter((entry) => entry.isFile())
    .map((entry) => entry.name);

  const checksumFiles = entries.filter((name) => /^SHA256SUMS/i.test(name));
  const packagedBinaries = entries.filter(
    (name) =>
      name.startsWith('mn42-bridge-') &&
      !name.endsWith('.txt') &&
      !name.endsWith('.json') &&
      !name.endsWith('.md'),
  );
  const packagedPrograms = packagedBinaries.map((name) => ({
    filename: name,
    role: name.includes('-console-')
      ? 'console'
      : name.includes('-cli-')
        ? 'cli'
        : 'unknown',
  }));

  const manifest = {
    target,
    versionTag: releaseTag,
    nodeTarget,
    commitSha,
    createdAt: new Date().toISOString(),
    checksumPaths: checksumFiles,
    packagedBinaries,
    packagedPrograms,
    signingStatus,
    signing: {
      status: signingStatus,
      identity: signingIdentity,
      notarizationStatus,
      note: signingNote,
    },
  };

  const outPath = path.join(stageDir, 'bridge_artifact_manifest.json');
  fs.writeFileSync(outPath, `${JSON.stringify(manifest, null, 2)}\n`);
  process.stdout.write(`${outPath}\n`);
}

try {
  main();
} catch (error) {
  console.error(error.message || error);
  process.exit(1);
}
