function normalizeBridgeRuntimeConfig(
  config,
  {
    parsePositiveInt,
    defaultFeedbackWindowMs,
    defaultRtP95TargetMs,
    defaultRtJitterP95TargetMs,
    defaultAlertSuppressionMs,
  } = {},
) {
  config.feedbackWindowMs = parsePositiveInt(
    config.feedbackWindowMs,
    defaultFeedbackWindowMs,
  );
  config.rtP95TargetMs = parsePositiveInt(
    config.rtP95TargetMs,
    defaultRtP95TargetMs,
  );
  config.rtJitterP95TargetMs = parsePositiveInt(
    config.rtJitterP95TargetMs,
    defaultRtJitterP95TargetMs,
  );
  config.alertSuppressionMs = parsePositiveInt(
    config.alertSuppressionMs,
    defaultAlertSuppressionMs,
  );
  return config;
}

async function startBridgeRuntime({
  isRunning,
  getState,
  loadDeps,
  setRuntimeFlags,
  resetTraceSeq,
  clearFeedbackGuard,
  clearMidiRpnState,
  resetRouteAlerts,
  clearPerformanceTracking,
  setState,
  createTimingState,
  createPerformanceState,
  createAlertState,
  config,
  clone,
  attachOsc,
  attachMidi,
  attachSerial,
} = {}) {
  if (isRunning()) return getState();
  await loadDeps();
  setRuntimeFlags({ stopping: false, manualStop: false, running: true });
  resetTraceSeq();
  clearFeedbackGuard();
  clearMidiRpnState();
  resetRouteAlerts();
  clearPerformanceTracking();
  setState({
    running: true,
    ready: false,
    lastError: null,
    lastRouteAt: null,
    lastRouteTraceId: null,
    routes: [],
    timing: createTimingState(),
    performance: createPerformanceState({
      rtP95TargetMs: config.rtP95TargetMs,
      rtJitterP95TargetMs: config.rtJitterP95TargetMs,
    }),
    alerts: createAlertState(),
    config: clone(config),
  });
  attachOsc();
  attachMidi();
  attachSerial();
  return getState();
}

async function stopBridgeRuntime({
  setRuntimeFlags,
  cancelSerialReconnect,
  cancelMidiRetry,
  getParser,
  setParser,
  getSerial,
  setSerial,
  getUdp,
  setUdp,
  getMidiIn,
  setMidiIn,
  getMidiOut,
  setMidiOut,
  detachListeners,
  closeSerialDevice,
  closeQuietly,
  pushLog,
  setState,
  config,
  clone,
  getState,
} = {}) {
  setRuntimeFlags({ manualStop: true, stopping: true, running: false });
  cancelSerialReconnect();
  cancelMidiRetry();

  detachListeners(getParser());
  setParser(null);

  detachListeners(getSerial());
  await closeSerialDevice(getSerial(), {
    onCloseError: (err) => {
      pushLog('error', `serial close failed: ${err.message}`);
    },
  });
  setSerial(null);

  closeQuietly(getUdp());
  setUdp(null);

  closeQuietly(getMidiIn());
  setMidiIn(null);

  closeQuietly(getMidiOut());
  setMidiOut(null);

  setState({
    running: false,
    serialConnected: false,
    ready: false,
    config: clone(config),
  });
  setRuntimeFlags({ stopping: false });
  return getState();
}

async function configureBridgeRuntime(
  nextConfig = {},
  { restart = true } = {},
  {
    config,
    normalizeConfig,
    setState,
    clone,
    refreshPerformance,
    isRunning,
    stop,
    start,
    clearFeedbackGuard,
    now = () => Date.now(),
  } = {},
) {
  Object.assign(config, nextConfig || {});
  normalizeConfig?.(config);
  if (config.allowFeedbackLoops) {
    clearFeedbackGuard();
  }
  setState({ config: clone(config) });
  refreshPerformance(now(), false);
  if (restart && isRunning()) {
    await stop();
    await start();
  }
  return {
    restartRequested: restart,
  };
}

module.exports = {
  normalizeBridgeRuntimeConfig,
  startBridgeRuntime,
  stopBridgeRuntime,
  configureBridgeRuntime,
};
