function createBridgeStateStore({
  events,
  state,
  clone,
  getConfig,
  logLimit = 200,
  stateEmitIntervalMs = 100,
} = {}) {
  let stateEmitTimer = null;
  function getState() {
    return {
      running: state.running,
      serialConnected: state.serialConnected,
      ready: state.ready,
      manifest: clone(state.manifest),
      deviceSession: clone(state.deviceSession),
      lastError: state.lastError,
      lastTelemetryAt: state.lastTelemetryAt,
      lastRouteAt: state.lastRouteAt,
      lastRouteTraceId: state.lastRouteTraceId,
      timing: clone(state.timing),
      performance: clone(state.performance),
      alerts: clone(state.alerts),
      logs: clone(state.logs),
      routes: clone(state.routes),
      counters: clone(state.counters),
      config: clone(getConfig()),
    };
  }

  function emitStateNow() {
    events.emit('state', getState());
  }

  // Route/metric traffic can arrive far faster than a browser can consume a
  // deep-cloned full snapshot. Coalesce snapshots to 10 Hz; structured route
  // and log events are still emitted immediately by their owning modules.
  function emitState() {
    if (stateEmitTimer) return;
    stateEmitTimer = setTimeout(() => {
      stateEmitTimer = null;
      emitStateNow();
    }, stateEmitIntervalMs);
    stateEmitTimer.unref?.();
  }

  function pushLog(level, message, extra = undefined) {
    const entry = {
      at: new Date().toISOString(),
      level,
      message,
      extra: extra === undefined ? undefined : clone(extra),
    };
    state.logs.push(entry);
    if (state.logs.length > logLimit) {
      state.logs.splice(0, state.logs.length - logLimit);
    }
    events.emit('log', entry);
    emitState();
  }

  function setState(partial) {
    Object.assign(state, partial);
    emitState();
  }

  function bumpCounter(name, amount = 1) {
    if (!Object.prototype.hasOwnProperty.call(state.counters, name)) return;
    state.counters[name] += amount;
    emitState();
  }

  return {
    emitState,
    emitStateNow,
    pushLog,
    setState,
    bumpCounter,
    getState,
  };
}

module.exports = {
  createBridgeStateStore,
};
