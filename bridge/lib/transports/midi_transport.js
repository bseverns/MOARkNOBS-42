function createMidiTransportLifecycle({
  getJzzFactory,
  getConfig,
  getRuntimeState,
  setMidiIn,
  setMidiOut,
  pushLog,
  createMessageHandler,
} = {}) {
  let retryTimer = null;
  const loggedFailureKeys = new Set();

  function shouldStayRunning() {
    const runtime = getRuntimeState?.() || {};
    return Boolean(runtime.running && !runtime.stopping && !runtime.manualStop);
  }

  function cancelRetry() {
    if (!retryTimer) return;
    clearTimeout(retryTimer);
    retryTimer = null;
  }

  function scheduleRetry() {
    if (retryTimer) return;
    retryTimer = setTimeout(() => {
      retryTimer = null;
      if (!shouldStayRunning()) return;
      attachMidi();
    }, 1000);
  }

  function formatPortNames(ports = []) {
    if (!Array.isArray(ports) || !ports.length) return 'none';
    return ports
      .map((port) => port?.name || port?.id)
      .filter(Boolean)
      .join(', ');
  }

  function midiInfo(midi) {
    try {
      return midi && typeof midi.info === 'function' ? midi.info() : {};
    } catch (_) {
      return {};
    }
  }

  function failureMessage(direction, label, err, info) {
    const names =
      direction === 'out'
        ? formatPortNames(info.outputs)
        : formatPortNames(info.inputs);
    const target = label || 'first available port';
    const detail = err ? `: ${err}` : '';
    return `MIDI ${direction} "${target}" failed${detail}. Available ${
      direction === 'out' ? 'outputs' : 'inputs'
    }: ${names}`;
  }

  function logOpenFailure(direction, label, err, info) {
    const message = failureMessage(direction, label, err, info);
    const key = `${direction}:${label || ''}:${err || ''}:${message}`;
    if (!loggedFailureKeys.has(key)) {
      pushLog?.('error', message);
      loggedFailureKeys.add(key);
    }
    scheduleRetry();
  }

  function attachMidi() {
    const jzzFactory = getJzzFactory?.();
    const config = getConfig?.() || {};
    const midi = jzzFactory();
    const info = midiInfo(midi);
    const midiLabel = config.midiLabel || undefined;
    cancelRetry();

    const midiOut = midi.openMidiOut(midiLabel).or(function onMidiOutError() {
      setMidiOut?.(null);
      const err = typeof this?._err === 'function' ? this._err() : '';
      logOpenFailure('out', midiLabel, err, info);
    });
    setMidiOut?.(midiOut);
    if (midiOut && typeof midiOut.on === 'function') {
      midiOut.on('error', (err) => {
        pushLog?.('error', `MIDI out error: ${err.message}`);
      });
    }

    const midiIn = midi.openMidiIn(midiLabel).or(function onMidiInError() {
      setMidiIn?.(null);
      const err = typeof this?._err === 'function' ? this._err() : '';
      logOpenFailure('in', midiLabel, err, info);
    });
    setMidiIn?.(midiIn);
    if (midiIn && typeof midiIn.on === 'function') {
      midiIn.on('error', (err) => {
        pushLog?.('error', `MIDI in error: ${err.message}`);
      });
    }

    if (midiIn && typeof midiIn.connect === 'function') {
      const handleMidiMessage = createMessageHandler?.();
      if (handleMidiMessage) {
        midiIn.connect(handleMidiMessage);
      }
    }
  }

  return {
    attachMidi,
    cancelRetry,
  };
}

module.exports = {
  createMidiTransportLifecycle,
};
