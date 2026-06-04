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
    transportLaneChip = null,
    connectFailHelp = null,
    usbMidiToggleBtn = null,
    usbMidiTestBtn = null,
    usbMidiStatusEl = null,
    noteDynamicsVelocityInput = null,
    noteDynamicsProbabilityInput = null,
    noteDynamicsApplyBtn = null,
    noteDynamicsStatusEl = null,
    jitterDepthInput = null,
    jitterSmoothnessInput = null,
    jitterApplyBtn = null,
    jitterStatusEl = null,
    deviceClockSourceSelect = null,
    deviceClockBpmInput = null,
    deviceClockOutSelect = null,
    deviceClockApplyBtn = null,
    deviceClockStatusEl = null
  } = elements;

  let lastCompatibilityReport = null;
  let usbMidiToggleSupported = false;
  let usbMidiBusy = false;
  let usbMidiOutEnabled = false;
  let noteDynamicsSupported = false;
  let noteDynamicsBusy = false;
  let noteDynamicsDraftDirty = false;
  let velocityShift = 0;
  let changeProbability = 100;
  let jitterSupported = false;
  let jitterBusy = false;
  let jitterDepth = 1;
  let jitterSmoothness = 0.5;
  let clockSupported = false;
  let clockBusy = false;
  let clockFollowExternal = true;
  let clockOutEnabled = false;
  let clockTappedBpm = 120;
  let clockExternalBpm = 0;
  let clockSource = 'idle';
  let clockRunning = false;
  let clockExternalSignal = false;

  function currentTransportMode() {
    return (
      runtime?.getState?.()?.transportMode ??
      (runtimeOptions.wsUrl ? 'bridge-raw' : 'direct-webserial')
    );
  }

  function usingSimulatorTransport() {
    return (
      currentTransportMode() === 'simulator' ||
      Boolean(simulatorToggle?.classList.contains('active'))
    );
  }

  function usingBridgeTransport() {
    return currentTransportMode().startsWith('bridge');
  }

  function usingDirectWebSerial() {
    return !usingBridgeTransport() && !usingSimulatorTransport();
  }

  function setConnectionBanner(stage, manifest) {
    if (!connectionBanner) return;
    const transportMode = currentTransportMode();
    const transportLabel =
      transportMode === 'bridge-session'
        ? ' via Bridge session'
        : transportMode === 'bridge-raw'
          ? ' via Bridge raw transport'
          : '';
    if (stage === 'live') {
      const deviceName = resolveDeviceName(manifest);
      const fwVersion = resolveFirmwareVersion(manifest);
      connectionBanner.textContent = `Connected to: ${deviceName} (FW ${fwVersion})${transportLabel}`;
      return;
    }
    if (stage === 'handshake') {
      connectionBanner.textContent = `Connecting to: ${resolveDeviceName(
        manifest
      )}${transportLabel}`;
      return;
    }
    connectionBanner.textContent = 'Connected to: —';
  }

  function updateTransportLaneChip() {
    if (!transportLaneChip) return;
    const transportMode = currentTransportMode();
    const labels = {
      'direct-webserial': 'Transport · Direct USB',
      simulator: 'Transport · Simulator',
      'bridge-raw': 'Transport · Bridge raw',
      'bridge-session': 'Transport · Bridge session'
    };
    transportLaneChip.dataset.transport = transportMode;
    transportLaneChip.textContent = labels[transportMode] ?? 'Transport · Direct USB';
  }

  function setConnectionPill(stage, text) {
    if (!connectionPill) return;
    connectionPill.dataset.stage = stage;
    connectionPill.textContent = text;
    if (docRoot) docRoot.dataset.connected = stage === 'live' ? 'true' : 'false';
    onConnectionPillChanged();
    updateTransportLaneChip();
  }

  function runCompatibilityCheck() {
    if (usingBridgeTransport()) {
      const bridgeModeMessage =
        currentTransportMode() === 'bridge-session'
          ? 'This session is using the bridge runtime and structured session cache, so direct Web Serial browser checks do not apply.'
          : 'This session is using the WebSocket bridge, so direct Web Serial browser checks do not apply.';
      setStatus('ok', 'Bridge mode active', bridgeModeMessage);
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

  function setNoteDynamicsStatus(state, message) {
    if (!noteDynamicsStatusEl) return;
    noteDynamicsStatusEl.dataset.state = state;
    noteDynamicsStatusEl.textContent = message;
  }

  function setJitterStatus(state, message) {
    if (!jitterStatusEl) return;
    jitterStatusEl.dataset.state = state;
    jitterStatusEl.textContent = message;
  }

  function setClockStatus(state, message) {
    if (!deviceClockStatusEl) return;
    deviceClockStatusEl.dataset.state = state;
    deviceClockStatusEl.textContent = message;
  }

  function syncInputValue(input, value) {
    if (!input) return;
    if (typeof document !== 'undefined' && document.activeElement === input) return;
    input.value = String(value);
  }

  function updateUsbMidiControls() {
    const connected = connectionPill?.dataset.stage === 'live';
    const disabled = !connected || usbMidiBusy || !usbMidiToggleSupported;
    if (usbMidiToggleBtn) {
      usbMidiToggleBtn.disabled = disabled;
      usbMidiToggleBtn.textContent = usbMidiOutEnabled ? 'USB MIDI On' : 'USB MIDI Off';
    }
    if (usbMidiTestBtn) {
      usbMidiTestBtn.disabled = disabled;
    }
  }

  function formatUsbMidiCounters(response) {
    const tx = Number(response?.tx_count ?? response?.tx_after);
    const rx = Number(response?.rx_count);
    const ticks = Number(response?.clock_ticks);
    const drops = Number(response?.midi_drops);
    const parts = [];
    if (Number.isFinite(tx)) parts.push(`tx ${tx}`);
    if (Number.isFinite(rx)) parts.push(`rx ${rx}`);
    if (Number.isFinite(ticks)) parts.push(`clock ${ticks}`);
    if (Number.isFinite(drops)) parts.push(`drops ${drops}`);
    return parts.length ? ` (${parts.join(' · ')})` : '';
  }

  function updateNoteDynamicsControls() {
    const connected = connectionPill?.dataset.stage === 'live';
    const disabled = !connected || noteDynamicsBusy || !noteDynamicsSupported;
    if (noteDynamicsVelocityInput) noteDynamicsVelocityInput.disabled = disabled;
    if (noteDynamicsProbabilityInput) noteDynamicsProbabilityInput.disabled = disabled;
    if (noteDynamicsApplyBtn) noteDynamicsApplyBtn.disabled = disabled;
    if (!noteDynamicsDraftDirty) {
      syncInputValue(noteDynamicsVelocityInput, velocityShift);
      syncInputValue(noteDynamicsProbabilityInput, changeProbability);
    }
  }

  function updateJitterControls() {
    const connected = connectionPill?.dataset.stage === 'live';
    const disabled = !connected || jitterBusy || !jitterSupported;
    if (jitterDepthInput) jitterDepthInput.disabled = disabled;
    if (jitterSmoothnessInput) jitterSmoothnessInput.disabled = disabled;
    if (jitterApplyBtn) jitterApplyBtn.disabled = disabled;
    syncInputValue(jitterDepthInput, jitterDepth.toFixed(2));
    syncInputValue(jitterSmoothnessInput, jitterSmoothness.toFixed(2));
  }

  function updateClockControls() {
    const connected = connectionPill?.dataset.stage === 'live';
    const disabled = !connected || clockBusy || !clockSupported;
    if (deviceClockSourceSelect) deviceClockSourceSelect.disabled = disabled;
    if (deviceClockBpmInput) deviceClockBpmInput.disabled = disabled;
    if (deviceClockOutSelect) deviceClockOutSelect.disabled = disabled;
    if (deviceClockApplyBtn) deviceClockApplyBtn.disabled = disabled;
    if (deviceClockSourceSelect && document.activeElement !== deviceClockSourceSelect) {
      deviceClockSourceSelect.value = clockFollowExternal ? 'external' : 'internal';
    }
    syncInputValue(deviceClockBpmInput, clockTappedBpm.toFixed(1));
    if (deviceClockOutSelect && document.activeElement !== deviceClockOutSelect) {
      deviceClockOutSelect.value = clockOutEnabled ? 'on' : 'off';
    }
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
        `${
          usbMidiOutEnabled ? 'USB MIDI output is enabled.' : 'USB MIDI output is disabled.'
        }${formatUsbMidiCounters(response)}`
      );
    } catch (err) {
      setUsbMidiStatus('err', `USB MIDI read failed: ${err.message || String(err)}`);
    } finally {
      usbMidiBusy = false;
      updateUsbMidiControls();
    }
  }

  async function refreshNoteDynamicsState() {
    if (!noteDynamicsSupported) {
      setNoteDynamicsStatus('muted', 'This firmware does not expose live note dynamics.');
      updateNoteDynamicsControls();
      return;
    }
    if (connectionPill?.dataset.stage !== 'live') {
      setNoteDynamicsStatus('muted', 'Connect to inspect live note dynamics.');
      updateNoteDynamicsControls();
      return;
    }
    noteDynamicsBusy = true;
    updateNoteDynamicsControls();
    setNoteDynamicsStatus('busy', 'Reading live note dynamics…');
    try {
      const response = await runtime.sendRpc(
        { rpc: 'get_note_dynamics' },
        { rollbackOnError: false }
      );
      velocityShift = Math.max(
        -64,
        Math.min(63, Math.round(Number(response?.velocity_shift) || 0))
      );
      changeProbability = Math.max(
        0,
        Math.min(100, Math.round(Number(response?.change_probability) || 0))
      );
      noteDynamicsDraftDirty = false;
      setNoteDynamicsStatus(
        'ok',
        `Velocity ${velocityShift >= 0 ? '+' : ''}${velocityShift} • Chance ${changeProbability}%`
      );
    } catch (err) {
      setNoteDynamicsStatus('err', `Note dynamics read failed: ${err.message || String(err)}`);
    } finally {
      noteDynamicsBusy = false;
      updateNoteDynamicsControls();
    }
  }

  async function refreshJitterState() {
    if (!jitterSupported) {
      setJitterStatus('muted', 'This firmware does not expose live jitter tuning.');
      updateJitterControls();
      return;
    }
    if (connectionPill?.dataset.stage !== 'live') {
      setJitterStatus('muted', 'Connect to inspect live jitter tuning.');
      updateJitterControls();
      return;
    }
    jitterBusy = true;
    updateJitterControls();
    setJitterStatus('busy', 'Reading live jitter tuning…');
    try {
      const response = await runtime.sendRpc({ rpc: 'get_jitter' }, { rollbackOnError: false });
      jitterDepth = Math.max(0, Math.min(1, Number(response?.depth) || 0));
      jitterSmoothness = Math.max(0, Math.min(1, Number(response?.smoothness) || 0));
      setJitterStatus(
        'ok',
        `Depth ${jitterDepth.toFixed(2)} • Smoothness ${jitterSmoothness.toFixed(2)}`
      );
    } catch (err) {
      setJitterStatus('err', `Jitter read failed: ${err.message || String(err)}`);
    } finally {
      jitterBusy = false;
      updateJitterControls();
    }
  }

  function formatClockStatus() {
    const sourceLabel = clockSource || (clockFollowExternal ? 'external' : 'internal');
    const extPart =
      Number.isFinite(clockExternalBpm) && clockExternalBpm > 0
        ? `Ext ${clockExternalBpm.toFixed(1)} BPM`
        : 'Ext -';
    return `${sourceLabel} • Int ${clockTappedBpm.toFixed(1)} BPM • ${extPart} • Out ${
      clockOutEnabled ? 'on' : 'off'
    }`;
  }

  async function refreshClockState() {
    if (!clockSupported) {
      setClockStatus('muted', 'This firmware does not expose live clock control.');
      updateClockControls();
      return;
    }
    if (connectionPill?.dataset.stage !== 'live') {
      setClockStatus('muted', 'Connect to inspect live device clock state.');
      updateClockControls();
      return;
    }
    clockBusy = true;
    updateClockControls();
    setClockStatus('busy', 'Reading device clock state…');
    try {
      const response = await runtime.sendRpc({ rpc: 'get_clock' }, { rollbackOnError: false });
      clockFollowExternal = Boolean(response?.follow_external);
      clockOutEnabled = Boolean(response?.clock_out_enabled);
      clockTappedBpm = Math.max(20, Math.min(300, Number(response?.tapped_bpm) || 120));
      clockExternalBpm = Math.max(0, Number(response?.external_bpm) || 0);
      clockSource =
        typeof response?.source === 'string'
          ? response.source
          : clockFollowExternal
            ? 'external'
            : 'internal';
      clockRunning = Boolean(response?.running);
      clockExternalSignal = Boolean(response?.external_signal);
      setClockStatus('ok', formatClockStatus());
    } catch (err) {
      setClockStatus('err', `Clock read failed: ${err.message || String(err)}`);
    } finally {
      clockBusy = false;
      updateClockControls();
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
      const response = await runtime.sendRpc(
        { rpc: 'set_usb_midi', enabled: !usbMidiOutEnabled },
        { rollbackOnError: false }
      );
      usbMidiOutEnabled = Boolean(response?.usb_midi_out);
      setUsbMidiStatus(
        'ok',
        `${
          usbMidiOutEnabled ? 'USB MIDI output enabled.' : 'USB MIDI output disabled.'
        }${formatUsbMidiCounters(response)}`
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

  async function runMidiTest() {
    if (!usbMidiToggleSupported || usbMidiBusy) return;
    if (connectionPill?.dataset.stage !== 'live') {
      setUsbMidiStatus('muted', 'Connect before running the MIDI test.');
      return;
    }
    usbMidiBusy = true;
    updateUsbMidiControls();
    setUsbMidiStatus('busy', 'Sending C4, CC1, and note-off over USB MIDI…');
    try {
      const response = await runtime.sendRpc({ rpc: 'midi_test' }, { rollbackOnError: false });
      usbMidiOutEnabled = Boolean(response?.usb_midi_out);
      setUsbMidiStatus('ok', `MIDI test sent.${formatUsbMidiCounters(response)}`);
      setStatus('ok', 'MIDI test sent', formatUsbMidiCounters(response).trim());
    } catch (err) {
      setUsbMidiStatus('err', `MIDI test failed: ${err.message || String(err)}`);
      setStatus('err', 'MIDI test failed', err.message || String(err));
    } finally {
      usbMidiBusy = false;
      updateUsbMidiControls();
    }
  }

  async function applyNoteDynamics() {
    if (!noteDynamicsSupported || noteDynamicsBusy) return;
    const nextVelocity = Math.max(
      -64,
      Math.min(63, Math.round(Number(noteDynamicsVelocityInput?.value) || 0))
    );
    const nextProbability = Math.max(
      0,
      Math.min(100, Math.round(Number(noteDynamicsProbabilityInput?.value) || 0))
    );
    noteDynamicsBusy = true;
    updateNoteDynamicsControls();
    setNoteDynamicsStatus('busy', 'Updating live note dynamics…');
    try {
      const response = await runtime.sendRpc(
        {
          rpc: 'set_note_dynamics',
          velocityShift: nextVelocity,
          changeProbability: nextProbability
        },
        { rollbackOnError: false }
      );
      velocityShift = Math.max(
        -64,
        Math.min(63, Math.round(Number(response?.velocity_shift) || 0))
      );
      changeProbability = Math.max(
        0,
        Math.min(100, Math.round(Number(response?.change_probability) || 0))
      );
      noteDynamicsDraftDirty = false;
      setNoteDynamicsStatus(
        'ok',
        `Velocity ${velocityShift >= 0 ? '+' : ''}${velocityShift} • Chance ${changeProbability}%`
      );
      setStatus(
        'ok',
        'Note dynamics updated',
        'Live note velocity and chance updated on the device.'
      );
    } catch (err) {
      setNoteDynamicsStatus('err', `Note dynamics update failed: ${err.message || String(err)}`);
      setStatus('err', 'Note dynamics update failed', err.message || String(err));
    } finally {
      noteDynamicsBusy = false;
      updateNoteDynamicsControls();
    }
  }

  async function applyJitter() {
    if (!jitterSupported || jitterBusy) return;
    const nextDepth = Math.max(0, Math.min(1, Number(jitterDepthInput?.value) || 0));
    const nextSmoothness = Math.max(0, Math.min(1, Number(jitterSmoothnessInput?.value) || 0));
    jitterBusy = true;
    updateJitterControls();
    setJitterStatus('busy', 'Updating live jitter tuning…');
    try {
      const response = await runtime.sendRpc(
        {
          rpc: 'set_jitter',
          depth: nextDepth,
          smoothness: nextSmoothness
        },
        { rollbackOnError: false }
      );
      jitterDepth = Math.max(0, Math.min(1, Number(response?.depth) || 0));
      jitterSmoothness = Math.max(0, Math.min(1, Number(response?.smoothness) || 0));
      setJitterStatus(
        'ok',
        `Depth ${jitterDepth.toFixed(2)} • Smoothness ${jitterSmoothness.toFixed(2)}`
      );
      setStatus('ok', 'Jitter updated', 'Live jitter tuning updated on the device.');
    } catch (err) {
      setJitterStatus('err', `Jitter update failed: ${err.message || String(err)}`);
      setStatus('err', 'Jitter update failed', err.message || String(err));
    } finally {
      jitterBusy = false;
      updateJitterControls();
    }
  }

  async function applyClockSettings() {
    if (!clockSupported || clockBusy) return;
    const followExternal = deviceClockSourceSelect?.value !== 'internal';
    const nextClockOutEnabled = deviceClockOutSelect?.value === 'on';
    const nextTappedBpm = Math.max(20, Math.min(300, Number(deviceClockBpmInput?.value) || 120));
    clockBusy = true;
    updateClockControls();
    setClockStatus('busy', 'Updating device clock state…');
    try {
      const response = await runtime.sendRpc(
        {
          rpc: 'set_clock',
          followExternal,
          clockOutEnabled: nextClockOutEnabled,
          tappedBpm: nextTappedBpm
        },
        { rollbackOnError: false }
      );
      clockFollowExternal = Boolean(response?.follow_external);
      clockOutEnabled = Boolean(response?.clock_out_enabled);
      clockTappedBpm = Math.max(20, Math.min(300, Number(response?.tapped_bpm) || nextTappedBpm));
      clockExternalBpm = Math.max(0, Number(response?.external_bpm) || 0);
      clockSource =
        typeof response?.source === 'string'
          ? response.source
          : clockFollowExternal
            ? 'external'
            : 'internal';
      clockRunning = Boolean(response?.running);
      clockExternalSignal = Boolean(response?.external_signal);
      setClockStatus('ok', formatClockStatus());
      setStatus('ok', 'Clock updated', 'Live device clock settings updated.');
    } catch (err) {
      setClockStatus('err', `Clock update failed: ${err.message || String(err)}`);
      setStatus('err', 'Clock update failed', err.message || String(err));
    } finally {
      clockBusy = false;
      updateClockControls();
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
      updateTransportLaneChip();
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
    usbMidiTestBtn?.addEventListener('click', () => runMidiTest());
    noteDynamicsApplyBtn?.addEventListener('click', () => applyNoteDynamics());
    noteDynamicsVelocityInput?.addEventListener('input', () => {
      noteDynamicsDraftDirty = true;
      updateNoteDynamicsControls();
    });
    noteDynamicsProbabilityInput?.addEventListener('input', () => {
      noteDynamicsDraftDirty = true;
      updateNoteDynamicsControls();
    });
    jitterApplyBtn?.addEventListener('click', () => applyJitter());
    deviceClockApplyBtn?.addEventListener('click', () => applyClockSettings());
    initializeSimulatorToggle();
    updateUsbMidiControls();
    updateNoteDynamicsControls();
    updateJitterControls();
    updateClockControls();
    updateTransportLaneChip();
  }

  function onManifest(manifest) {
    const capabilities =
      manifest?.capabilities && typeof manifest.capabilities === 'object'
        ? manifest.capabilities
        : {};
    clockSupported = Boolean(capabilities.clock_live);
    noteDynamicsSupported = Boolean(capabilities.note_dynamics_live);
    jitterSupported = Boolean(capabilities.jitter_live);
    usbMidiToggleSupported = Boolean(capabilities.usb_midi_toggle);
    if (!usbMidiToggleSupported) {
      setUsbMidiStatus('muted', 'This firmware does not expose USB MIDI toggling.');
    }
    if (!noteDynamicsSupported) {
      setNoteDynamicsStatus('muted', 'This firmware does not expose live note dynamics.');
    }
    if (!jitterSupported) {
      setJitterStatus('muted', 'This firmware does not expose live jitter tuning.');
    }
    if (!clockSupported) {
      setClockStatus('muted', 'This firmware does not expose live clock control.');
    }
    updateUsbMidiControls();
    updateNoteDynamicsControls();
    updateJitterControls();
    updateClockControls();
  }

  function onConnected() {
    void refreshUsbMidiState();
    if (noteDynamicsSupported) {
      void refreshNoteDynamicsState();
    }
    if (jitterSupported) {
      setJitterStatus('muted', 'Waiting for live jitter telemetry…');
      updateJitterControls();
    }
    if (clockSupported) {
      setClockStatus('muted', 'Waiting for live device clock telemetry…');
      updateClockControls();
    }
  }

  function onDisconnected() {
    usbMidiBusy = false;
    usbMidiOutEnabled = false;
    noteDynamicsBusy = false;
    noteDynamicsDraftDirty = false;
    jitterBusy = false;
    clockBusy = false;
    setUsbMidiStatus('muted', 'Connect to inspect USB MIDI output state.');
    setNoteDynamicsStatus('muted', 'Connect to inspect live note dynamics.');
    setJitterStatus('muted', 'Connect to inspect live jitter tuning.');
    setClockStatus('muted', 'Connect to inspect live device clock state.');
    updateUsbMidiControls();
    updateNoteDynamicsControls();
    updateJitterControls();
    updateClockControls();
  }

  function onTelemetry(frame) {
    if (frame?.note_dynamics && typeof frame.note_dynamics === 'object') {
      if (!noteDynamicsDraftDirty) {
        velocityShift = Math.max(
          -64,
          Math.min(63, Math.round(Number(frame.note_dynamics.velocity_shift) || 0))
        );
        changeProbability = Math.max(
          0,
          Math.min(100, Math.round(Number(frame.note_dynamics.change_probability) || 0))
        );
      }
      if (
        !noteDynamicsBusy &&
        !noteDynamicsDraftDirty &&
        noteDynamicsSupported &&
        connectionPill?.dataset.stage === 'live'
      ) {
        setNoteDynamicsStatus(
          'ok',
          `Velocity ${velocityShift >= 0 ? '+' : ''}${velocityShift} • Chance ${changeProbability}%`
        );
      }
      updateNoteDynamicsControls();
    }
    if (frame?.jitter && typeof frame.jitter === 'object') {
      jitterDepth = Math.max(0, Math.min(1, Number(frame.jitter.depth) || 0));
      jitterSmoothness = Math.max(0, Math.min(1, Number(frame.jitter.smoothness) || 0));
      if (!jitterBusy && jitterSupported && connectionPill?.dataset.stage === 'live') {
        setJitterStatus(
          'ok',
          `Depth ${jitterDepth.toFixed(2)} • Smoothness ${jitterSmoothness.toFixed(2)}`
        );
      }
      updateJitterControls();
    }
    if (frame?.clock && typeof frame.clock === 'object') {
      clockFollowExternal = Boolean(frame.clock.follow_external);
      clockOutEnabled = Boolean(frame.clock.clock_out_enabled);
      clockTappedBpm = Math.max(20, Math.min(300, Number(frame.clock.tapped_bpm) || 120));
      clockExternalBpm = Math.max(0, Number(frame.clock.external_bpm) || 0);
      clockSource =
        typeof frame.clock.source === 'string'
          ? frame.clock.source
          : clockFollowExternal
            ? 'external'
            : 'internal';
      clockRunning = Boolean(frame.clock.running);
      clockExternalSignal = Boolean(frame.clock.external_signal);
      if (!clockBusy && clockSupported && connectionPill?.dataset.stage === 'live') {
        setClockStatus('ok', formatClockStatus());
      }
      updateClockControls();
    }
  }

  return {
    bind,
    connect,
    onManifest,
    onConnected,
    onDisconnected,
    onTelemetry,
    setConnectionBanner,
    setConnectionPill,
    updateTransportLaneChip,
    primeCompatibilityStatus,
    syncConfigFileButtons
  };
}
