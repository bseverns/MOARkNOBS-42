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

  function attachMidi() {
    const jzzFactory = getJzzFactory?.();
    const config = getConfig?.() || {};
    const midi = jzzFactory();
    cancelRetry();

    const midiOut = midi.openMidiOut(config.midiLabel).or(() => {
      pushLog?.('error', 'MIDI out failed');
      scheduleRetry();
    });
    setMidiOut?.(midiOut);
    if (midiOut && typeof midiOut.on === 'function') {
      midiOut.on('error', (err) => {
        pushLog?.('error', `MIDI out error: ${err.message}`);
      });
    }

    const midiIn = midi.openMidiIn(config.midiLabel).or(() => {
      pushLog?.('error', 'MIDI in failed');
      scheduleRetry();
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
