function createOscTransport({
  getOscApi,
  getConfig,
  getRuntimeState,
  setUdp,
  pushLog,
  createMessageHandler,
} = {}) {
  function shouldStayRunning() {
    const runtime = getRuntimeState?.() || {};
    return Boolean(runtime.running && !runtime.stopping && !runtime.manualStop);
  }

  function attachOsc() {
    const oscApi = getOscApi?.();
    const config = getConfig?.() || {};
    const udp = new oscApi.UDPPort({
      localAddress: config.oscBind,
      localPort: config.oscListen,
    });
    setUdp?.(udp);

    udp.on('error', (err) => {
      pushLog?.('error', `udp error: ${err.message}`);
      try {
        udp.close();
      } catch (_) {
        // no-op
      }
      setTimeout(() => {
        if (!shouldStayRunning()) return;
        try {
          udp.open();
        } catch (openErr) {
          pushLog?.('error', `udp reopen failed: ${openErr.message}`);
        }
      }, 1000);
    });

    const handleOscMessage = createMessageHandler?.();
    if (handleOscMessage) {
      udp.on('message', handleOscMessage);
    }

    udp.open();
  }

  return {
    attachOsc,
  };
}

module.exports = {
  createOscTransport,
};
