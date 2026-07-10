#!/usr/bin/env node

const REQUIRED_NODE_MAJOR = 24;

function parseMajor(version) {
  const normalized = String(version || '').trim().replace(/^v/, '');
  const [major] = normalized.split('.');
  const parsed = Number(major);
  return Number.isInteger(parsed) ? parsed : null;
}

function validateNodeVersion(version = process.versions.node) {
  const major = parseMajor(version);
  return {
    ok: major === REQUIRED_NODE_MAJOR,
    version: String(version || 'unknown'),
    major,
    requiredMajor: REQUIRED_NODE_MAJOR,
  };
}

function run(version = process.versions.node, { stderr = process.stderr } = {}) {
  const result = validateNodeVersion(version);
  if (result.ok) return 0;
  stderr.write(
    [
      `MOARkNOBS-42 App/Bridge test and CI scripts require Node ${REQUIRED_NODE_MAJOR}.x;`,
      `current Node is ${result.version}.`,
      `Switch to Node ${REQUIRED_NODE_MAJOR} before running this command.`,
    ].join(' ') + '\n',
  );
  return 1;
}

if (require.main === module) {
  process.exitCode = run();
}

module.exports = {
  REQUIRED_NODE_MAJOR,
  parseMajor,
  run,
  validateNodeVersion,
};
