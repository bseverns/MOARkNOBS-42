const { loadBridgeSettingsFileSync } = require('./settings_file');

// Shared CLI/server help text so both bridge entrypoints describe the same contract.
function usageText() {
  return (
    'mn42_bridge.js - link MOARkNOBS-42 to OSC & MIDI\n' +
    'Usage: node mn42_bridge.js [--config FILE] [--serial PORT] [--osc PORT] [--osc-listen PORT] [--host ADDR] [--bind ADDR] [--midi LABEL] [--unsafe-network-http] [--allow-feedback-loops] [--feedback-window-ms N] [--rt-p95-target-ms N] [--rt-jitter-p95-target-ms N] [--alert-suppression-ms N]'
  );
}

// Minimal argv reader; the bridge keeps parsing intentionally lightweight.
function getArg(argv, flag, def) {
  const idx = argv.indexOf(flag);
  if (idx >= 0 && idx + 1 < argv.length) return argv[idx + 1];
  return def;
}

// Parse port-like CLI values and fall back cleanly when they are absent or invalid.
function parsePositiveInt(raw, fallback) {
  const parsed = parseInt(String(raw), 10);
  if (!Number.isInteger(parsed) || parsed <= 0) return fallback;
  return parsed;
}

// Translate CLI flags into the bridge's runtime config object.
function parseConfigFromArgv(argv = process.argv, defaults = {}) {
  const {
    defaultOscPort,
    defaultFeedbackWindowMs,
    defaultRtP95TargetMs,
    defaultRtJitterP95TargetMs,
    defaultAlertSuppressionMs,
    defaultHttpPort,
  } = defaults;

  const configPathArg = getArg(argv, '--config', getArg(argv, '-c', null));
  const settingsFile = configPathArg
    ? loadBridgeSettingsFileSync(configPathArg)
    : { path: null, config: {} };
  const fileConfig = settingsFile.config;

  const oscPort = parsePositiveInt(
    getArg(
      argv,
      '--osc',
      getArg(argv, '-o', String(fileConfig.oscPort ?? defaultOscPort)),
    ),
    null,
  );
  if (oscPort === null) {
    const error = new Error('bad --osc port');
    error.showUsage = true;
    throw error;
  }

  const oscListen = parsePositiveInt(
    getArg(
      argv,
      '--osc-listen',
      String(fileConfig.oscListen ?? fileConfig.oscPort ?? defaultOscPort),
    ),
    fileConfig.oscListen ?? fileConfig.oscPort ?? defaultOscPort,
  );
  const feedbackWindowMs = parsePositiveInt(
    getArg(
      argv,
      '--feedback-window-ms',
      String(fileConfig.feedbackWindowMs ?? defaultFeedbackWindowMs),
    ),
    null,
  );
  if (feedbackWindowMs === null) {
    const error = new Error('bad --feedback-window-ms');
    error.showUsage = true;
    throw error;
  }
  const rtP95TargetMs = parsePositiveInt(
    getArg(
      argv,
      '--rt-p95-target-ms',
      String(fileConfig.rtP95TargetMs ?? defaultRtP95TargetMs),
    ),
    null,
  );
  if (rtP95TargetMs === null) {
    const error = new Error('bad --rt-p95-target-ms');
    error.showUsage = true;
    throw error;
  }
  const rtJitterP95TargetMs = parsePositiveInt(
    getArg(
      argv,
      '--rt-jitter-p95-target-ms',
      String(fileConfig.rtJitterP95TargetMs ?? defaultRtJitterP95TargetMs),
    ),
    null,
  );
  if (rtJitterP95TargetMs === null) {
    const error = new Error('bad --rt-jitter-p95-target-ms');
    error.showUsage = true;
    throw error;
  }
  const alertSuppressionMs = parsePositiveInt(
    getArg(
      argv,
      '--alert-suppression-ms',
      String(fileConfig.alertSuppressionMs ?? defaultAlertSuppressionMs),
    ),
    null,
  );
  if (alertSuppressionMs === null) {
    const error = new Error('bad --alert-suppression-ms');
    error.showUsage = true;
    throw error;
  }

  return {
    configPath: settingsFile.path,
    serialName: getArg(
      argv,
      '--serial',
      getArg(argv, '-s', fileConfig.serialName ?? '/dev/ttyACM0'),
    ),
    oscPort,
    oscListen,
    oscHost: getArg(
      argv,
      '--host',
      getArg(argv, '-H', fileConfig.oscHost ?? '127.0.0.1'),
    ),
    oscBind: getArg(
      argv,
      '--bind',
      getArg(argv, '-b', fileConfig.oscBind ?? '127.0.0.1'),
    ),
    midiLabel: getArg(
      argv,
      '--midi',
      getArg(argv, '-m', fileConfig.midiLabel ?? 'MN42 Bridge'),
    ),
    httpPort: parsePositiveInt(
      getArg(
        argv,
        '--http-port',
        String(fileConfig.httpPort ?? defaultHttpPort),
      ),
      fileConfig.httpPort ?? defaultHttpPort,
    ),
    httpHost: getArg(argv, '--http-host', fileConfig.httpHost ?? '127.0.0.1'),
    allowNetworkHttp:
      argv.includes('--unsafe-network-http') || Boolean(fileConfig.allowNetworkHttp),
    allowFeedbackLoops:
      argv.includes('--allow-feedback-loops') ||
      Boolean(fileConfig.allowFeedbackLoops),
    feedbackWindowMs,
    rtP95TargetMs,
    rtJitterP95TargetMs,
    alertSuppressionMs,
    midiToOscMappings: fileConfig.midiToOscMappings ?? [],
  };
}

module.exports = {
  usageText,
  getArg,
  parsePositiveInt,
  parseConfigFromArgv,
};
