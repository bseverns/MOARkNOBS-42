const fs = require('node:fs');
const path = require('node:path');

const { normalizeMidiToOscMappings } = require('./midi_osc_mappings');

const ALLOWED_CONFIG_KEYS = new Set([
  'serialName',
  'oscPort',
  'oscListen',
  'oscHost',
  'oscBind',
  'midiLabel',
  'httpPort',
  'httpHost',
  'allowFeedbackLoops',
  'feedbackWindowMs',
  'rtP95TargetMs',
  'rtJitterP95TargetMs',
  'alertSuppressionMs',
  'midiToOscMappings',
]);

function resolveConfigPath(rawPath, cwd = process.cwd()) {
  if (typeof rawPath !== 'string' || !rawPath.trim()) return null;
  const trimmed = rawPath.trim();
  return path.isAbsolute(trimmed) ? trimmed : path.resolve(cwd, trimmed);
}

function pickAllowedKeys(raw = {}) {
  const next = {};
  for (const key of ALLOWED_CONFIG_KEYS) {
    if (raw[key] !== undefined) {
      next[key] = raw[key];
    }
  }
  if (next.midiToOscMappings !== undefined) {
    next.midiToOscMappings = normalizeMidiToOscMappings(next.midiToOscMappings);
  }
  return next;
}

function loadBridgeSettingsFileSync(rawPath, cwd = process.cwd()) {
  const resolvedPath = resolveConfigPath(rawPath, cwd);
  if (!resolvedPath) return { path: null, config: {} };
  let parsed;
  try {
    parsed = JSON.parse(fs.readFileSync(resolvedPath, 'utf8'));
  } catch (err) {
    const message =
      err && err.code === 'ENOENT'
        ? `bridge config file not found: ${resolvedPath}`
        : `failed to read bridge config file: ${resolvedPath}`;
    const error = new Error(message);
    error.cause = err;
    throw error;
  }
  if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) {
    throw new Error(
      `bridge config file must contain a JSON object: ${resolvedPath}`,
    );
  }
  return {
    path: resolvedPath,
    config: pickAllowedKeys(parsed),
  };
}

module.exports = {
  ALLOWED_CONFIG_KEYS,
  resolveConfigPath,
  loadBridgeSettingsFileSync,
};
