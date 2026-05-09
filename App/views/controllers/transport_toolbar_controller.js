import {
  getWebSerialCompatibility,
  explainTransportError
} from '../../runtime/web_serial_diagnostics.js';

export function createTransportToolbarController({
  runtime,
  runtimeOptions = {},
  elements = {},
  resolveDeviceName,
  resolveFirmwareVersion,
  setStatus,
  syncConfigFileButtons = () => {},
  onConnectionPillChanged = () => {}
} = {}) {
  const {
    docRoot = null,
    connectBtn = null,
    checkCompatibilityBtn = null,
    configModeBtn = null,
    applyBtn = null,
    rollbackBtn = null,
    simulatorToggle = null,
    connectionPill = null,
    connectionBanner = null,
    connectFailHelp = null
  } = elements;

  let lastCompatibilityReport = null;

  function usingSimulatorTransport() {
    return Boolean(simulatorToggle?.classList.contains('active'));
  }

  function usingBridgeTransport() {
    return Boolean(runtimeOptions.wsUrl);
  }

  function usingDirectWebSerial() {
    return !usingBridgeTransport() && !usingSimulatorTransport();
  }

  function setConnectionBanner(stage, manifest) {
    if (!connectionBanner) return;
    if (stage === 'live') {
      const deviceName = resolveDeviceName(manifest);
      const fwVersion = resolveFirmwareVersion(manifest);
      connectionBanner.textContent = `Connected to: ${deviceName} (FW ${fwVersion})`;
      return;
    }
    if (stage === 'handshake') {
      connectionBanner.textContent = `Connecting to: ${resolveDeviceName(manifest)}`;
      return;
    }
    connectionBanner.textContent = 'Connected to: —';
  }

  function setConnectionPill(stage, text) {
    if (!connectionPill) return;
    connectionPill.dataset.stage = stage;
    connectionPill.textContent = text;
    if (docRoot) docRoot.dataset.connected = stage === 'live' ? 'true' : 'false';
    onConnectionPillChanged();
  }

  function runCompatibilityCheck() {
    if (usingBridgeTransport()) {
      setStatus(
        'ok',
        'Bridge mode active',
        'This session is using the WebSocket bridge, so direct Web Serial browser checks do not apply.'
      );
      return null;
    }
    if (usingSimulatorTransport()) {
      setStatus(
        'ok',
        'Simulator active',
        'Simulator mode skips direct Web Serial requirements until you reconnect to hardware.'
      );
      return null;
    }
    lastCompatibilityReport = getWebSerialCompatibility();
    setStatus(
      lastCompatibilityReport.state,
      lastCompatibilityReport.label,
      lastCompatibilityReport.message
    );
    return lastCompatibilityReport;
  }

  function showTransportError(action, err) {
    const explanation = explainTransportError(err, {
      action,
      compatibility: usingDirectWebSerial() ? lastCompatibilityReport : null,
      usingBridge: usingBridgeTransport(),
      usingSimulator: usingSimulatorTransport()
    });
    connectFailHelp?.setAttribute('open', '');
    setStatus('err', explanation.label, explanation.message);
  }

  function primeCompatibilityStatus() {
    if (!usingDirectWebSerial()) return;
    lastCompatibilityReport = getWebSerialCompatibility();
    if (lastCompatibilityReport.compatible) return;
    connectFailHelp?.setAttribute('open', '');
    setStatus(
      lastCompatibilityReport.state,
      lastCompatibilityReport.label,
      lastCompatibilityReport.message
    );
  }

  async function connect() {
    try {
      if (usingDirectWebSerial()) {
        const compatibility = runCompatibilityCheck();
        if (compatibility && !compatibility.compatible) {
          setConnectionPill('disconnected', 'Disconnected');
          setConnectionBanner('disconnected', runtime.getState().manifest);
          connectFailHelp?.setAttribute('open', '');
          return;
        }
      }
      setConnectionPill('handshake', 'Handshaking…');
      setConnectionBanner('handshake', runtime.getState().manifest);
      await runtime.connect();
    } catch (err) {
      setConnectionPill('disconnected', 'Disconnected');
      setConnectionBanner('disconnected', runtime.getState().manifest);
      showTransportError('connect', err);
    }
  }

  async function requestConfigBoot() {
    try {
      if (configModeBtn) configModeBtn.disabled = true;
      if (usingDirectWebSerial()) {
        const compatibility = runCompatibilityCheck();
        if (compatibility && !compatibility.compatible) {
          setConnectionPill('disconnected', 'Disconnected');
          setConnectionBanner('disconnected', runtime.getState().manifest);
          connectFailHelp?.setAttribute('open', '');
          return;
        }
      }
      setConnectionPill('handshake', 'Config boot…');
      setConnectionBanner('handshake', runtime.getState().manifest);
      await runtime.requestConfiguratorBoot();
      setConnectionPill('disconnected', 'Rebooting');
      setConnectionBanner('disconnected', runtime.getState().manifest);
      setStatus('ok', 'Config boot', 'Reconnect after the USB device reappears.');
    } catch (err) {
      setConnectionPill('disconnected', 'Disconnected');
      setConnectionBanner('disconnected', runtime.getState().manifest);
      showTransportError('config-boot', err);
    } finally {
      if (configModeBtn) configModeBtn.disabled = false;
    }
  }

  async function apply() {
    try {
      setStatus('warn', 'Applying…', 'Waiting for firmware ACK');
      await runtime.apply();
      setStatus('ok', 'Synced', 'Device acknowledged the staged edits.');
    } catch (err) {
      setStatus('err', 'Apply failed', err.message || String(err));
    }
  }

  async function rollback() {
    await runtime.rollback();
    setStatus('warn', 'Rolled back', 'Local edits were discarded.');
  }

  function initializeSimulatorToggle() {
    if (!simulatorToggle) return;
    simulatorToggle.setAttribute(
      'aria-pressed',
      simulatorToggle.classList.contains('active') ? 'true' : 'false'
    );
    if (simulatorToggle.dataset.booted) return;
    simulatorToggle.dataset.booted = 'true';
    simulatorToggle.addEventListener('click', () => {
      const toggled = simulatorToggle.classList.toggle('active');
      runtime.useSimulator(toggled);
      simulatorToggle.textContent = toggled ? 'Stop simulator' : 'Start simulator';
      simulatorToggle.setAttribute('aria-pressed', toggled ? 'true' : 'false');
      setStatus(
        toggled ? 'ok' : 'warn',
        toggled ? 'Simulator armed' : 'Simulator idle',
        toggled ? 'Replay frames without hardware.' : 'Connect to the physical deck.'
      );
    });
  }

  function bind() {
    connectBtn?.addEventListener('click', () => connect());
    checkCompatibilityBtn?.addEventListener('click', () => runCompatibilityCheck());
    configModeBtn?.addEventListener('click', () => requestConfigBoot());
    applyBtn?.addEventListener('click', () => apply());
    rollbackBtn?.addEventListener('click', () => rollback());
    initializeSimulatorToggle();
  }

  return {
    bind,
    connect,
    setConnectionBanner,
    setConnectionPill,
    primeCompatibilityStatus,
    syncConfigFileButtons
  };
}
