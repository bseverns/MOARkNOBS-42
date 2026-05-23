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
    connectFailHelp = null,
    usbMidiToggleBtn = null,
    usbMidiStatusEl = null
  } = elements;

  let lastCompatibilityReport = null;
  let usbMidiToggleSupported = false;
  let usbMidiBusy = false;
  let usbMidiOutEnabled = false;

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

  function setUsbMidiStatus(state, message) {
    if (!usbMidiStatusEl) return;
    usbMidiStatusEl.dataset.state = state;
    usbMidiStatusEl.textContent = message;
  }

  function updateUsbMidiControls() {
    if (!usbMidiToggleBtn) return;
    const connected = connectionPill?.dataset.stage === 'live';
    usbMidiToggleBtn.disabled = !connected || usbMidiBusy || !usbMidiToggleSupported;
    usbMidiToggleBtn.textContent = usbMidiOutEnabled ? 'USB MIDI On' : 'USB MIDI Off';
  }

  async function refreshUsbMidiState() {
    if (!usbMidiToggleSupported) {
      setUsbMidiStatus('muted', 'This firmware does not expose USB MIDI toggling.');
      updateUsbMidiControls();
      return;
    }
    if (connectionPill?.dataset.stage !== 'live') {
      setUsbMidiStatus('muted', 'Connect to inspect USB MIDI output state.');
      updateUsbMidiControls();
      return;
    }
    usbMidiBusy = true;
    updateUsbMidiControls();
    setUsbMidiStatus('busy', 'Reading USB MIDI output state…');
    try {
      const response = await runtime.sendRpc({ rpc: 'get_usb_midi' }, { rollbackOnError: false });
      usbMidiOutEnabled = Boolean(response?.usb_midi_out);
      setUsbMidiStatus(
        'ok',
        usbMidiOutEnabled ? 'USB MIDI output is enabled.' : 'USB MIDI output is disabled.'
      );
    } catch (err) {
      setUsbMidiStatus('err', `USB MIDI read failed: ${err.message || String(err)}`);
    } finally {
      usbMidiBusy = false;
      updateUsbMidiControls();
    }
  }

  async function toggleUsbMidi() {
    if (!usbMidiToggleSupported || usbMidiBusy) return;
    usbMidiBusy = true;
    updateUsbMidiControls();
    setUsbMidiStatus(
      'busy',
      usbMidiOutEnabled ? 'Disabling USB MIDI output…' : 'Enabling USB MIDI output…'
    );
    try {
      const response = await runtime.sendRpc({ rpc: 'set_usb_midi', enabled: !usbMidiOutEnabled });
      usbMidiOutEnabled = Boolean(response?.usb_midi_out);
      setUsbMidiStatus(
        'ok',
        usbMidiOutEnabled ? 'USB MIDI output enabled.' : 'USB MIDI output disabled.'
      );
      setStatus(
        'ok',
        'USB MIDI updated',
        usbMidiOutEnabled ? 'USB MIDI output enabled.' : 'USB MIDI output disabled.'
      );
    } catch (err) {
      setUsbMidiStatus('err', `USB MIDI update failed: ${err.message || String(err)}`);
      setStatus('err', 'USB MIDI update failed', err.message || String(err));
    } finally {
      usbMidiBusy = false;
      updateUsbMidiControls();
    }
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
    usbMidiToggleBtn?.addEventListener('click', () => toggleUsbMidi());
    initializeSimulatorToggle();
    updateUsbMidiControls();
  }

  function onManifest(manifest) {
    const capabilities =
      manifest?.capabilities && typeof manifest.capabilities === 'object'
        ? manifest.capabilities
        : {};
    usbMidiToggleSupported = Boolean(capabilities.usb_midi_toggle);
    if (!usbMidiToggleSupported) {
      setUsbMidiStatus('muted', 'This firmware does not expose USB MIDI toggling.');
    }
    updateUsbMidiControls();
  }

  function onConnected() {
    void refreshUsbMidiState();
  }

  function onDisconnected() {
    usbMidiBusy = false;
    usbMidiOutEnabled = false;
    setUsbMidiStatus('muted', 'Connect to inspect USB MIDI output state.');
    updateUsbMidiControls();
  }

  return {
    bind,
    connect,
    onManifest,
    onConnected,
    onDisconnected,
    setConnectionBanner,
    setConnectionPill,
    primeCompatibilityStatus,
    syncConfigFileButtons
  };
}
