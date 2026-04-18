// Shared CLI/server help text so both bridge entrypoints describe the same contract.
function usageText() {
  return (
    'mn42_bridge.js - link MOARkNOBS-42 to OSC & MIDI\n' +
    'Usage: node mn42_bridge.js [--serial PORT] [--osc PORT] [--osc-listen PORT] [--host ADDR] [--bind ADDR] [--midi LABEL] [--allow-feedback-loops] [--feedback-window-ms N] [--rt-p95-target-ms N] [--rt-jitter-p95-target-ms N] [--alert-suppression-ms N]'
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

  const oscPort = parsePositiveInt(
    getArg(argv, '--osc', getArg(argv, '-o', String(defaultOscPort))),
    null,
  );
  if (oscPort === null) {
    const error = new Error('bad --osc port');
    error.showUsage = true;
    throw error;
  }

  const oscListen = parsePositiveInt(
    getArg(argv, '--osc-listen', String(defaultOscPort)),
    defaultOscPort,
  );
  const feedbackWindowMs = parsePositiveInt(
    getArg(argv, '--feedback-window-ms', String(defaultFeedbackWindowMs)),
    null,
  );
  if (feedbackWindowMs === null) {
    const error = new Error('bad --feedback-window-ms');
    error.showUsage = true;
    throw error;
  }
  const rtP95TargetMs = parsePositiveInt(
    getArg(argv, '--rt-p95-target-ms', String(defaultRtP95TargetMs)),
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
      String(defaultRtJitterP95TargetMs),
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
      String(defaultAlertSuppressionMs),
    ),
    null,
  );
  if (alertSuppressionMs === null) {
    const error = new Error('bad --alert-suppression-ms');
    error.showUsage = true;
    throw error;
  }

  return {
    serialName: getArg(argv, '--serial', getArg(argv, '-s', '/dev/ttyACM0')),
    oscPort,
    oscListen,
    oscHost: getArg(argv, '--host', getArg(argv, '-H', '127.0.0.1')),
    oscBind: getArg(argv, '--bind', getArg(argv, '-b', '127.0.0.1')),
    midiLabel: getArg(argv, '--midi', getArg(argv, '-m', 'MN42 Bridge')),
    httpPort: parsePositiveInt(
      getArg(argv, '--http-port', String(defaultHttpPort)),
      defaultHttpPort,
    ),
    httpHost: getArg(argv, '--http-host', '127.0.0.1'),
    allowFeedbackLoops: argv.includes('--allow-feedback-loops'),
    feedbackWindowMs,
    rtP95TargetMs,
    rtJitterP95TargetMs,
    alertSuppressionMs,
  };
}

module.exports = {
  usageText,
  getArg,
  parsePositiveInt,
  parseConfigFromArgv,
};
