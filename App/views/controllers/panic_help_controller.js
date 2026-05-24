function formatCount(value, singular, plural = `${singular}s`) {
  const numeric = Number(value);
  if (!Number.isFinite(numeric) || numeric < 0) return `0 ${plural}`;
  return `${numeric} ${numeric === 1 ? singular : plural}`;
}

export function createPanicHelpController({
  runtime,
  localManifest = {},
  resolveDeviceName = () => 'MOARkNOBS-42',
  resolveFirmwareVersion = () => 'unknown',
  getConnectionStage = () => 'disconnected',
  getSessionLogCount = () => 0,
  isSimulatorActive = () => false,
  getConfigBootDisabled = () => false,
  onExportConfig = () => {},
  onExportSessionLog = () => {},
  onRequestConfiguratorBoot = () => {},
  elements = {}
} = {}) {
  const {
    dialog = null,
    closeBtn = null,
    contextEl = null,
    preflightEl = null,
    verifyTargetEl = null,
    exportConfigBtn = null,
    exportLogBtn = null,
    configBootBtn = null
  } = elements;

  function render() {
    const state = runtime?.getState?.() ?? {};
    const manifest = state.manifest ?? localManifest;
    const connected = getConnectionStage() === 'live';
    const dirty = Boolean(state.dirty);
    const hasStagedConfig = Boolean(state.staged);
    const simulator = Boolean(isSimulatorActive());
    const sessionLogCount = Number(getSessionLogCount()) || 0;
    const deviceName = resolveDeviceName(manifest);
    const firmwareVersion = resolveFirmwareVersion(manifest);
    const schemaVersion = manifest?.schema_version ?? localManifest?.schema_version ?? '?';

    if (contextEl) {
      if (simulator) {
        contextEl.textContent =
          'Simulator is active. Use this as rehearsal only; flashing and configurator reboot require a physical device.';
      } else if (dirty) {
        contextEl.textContent =
          'Apply or roll back staged edits before flashing so the browser snapshot and device state do not drift.';
      } else if (connected) {
        contextEl.textContent =
          'Live device is connected. Export a backup first, flash the firmware, then reconnect here to verify the new handshake.';
      } else {
        contextEl.textContent =
          'No live device is connected. You can still export browser-side backups before running the external flash tools.';
      }
    }

    if (preflightEl) {
      preflightEl.textContent = [
        connected ? 'Connected' : 'Offline',
        dirty
          ? 'staged edits dirty'
          : hasStagedConfig
            ? 'staged snapshot clean'
            : 'no staged config',
        hasStagedConfig ? 'backup ready' : 'no config backup',
        `${formatCount(sessionLogCount, 'log entry')} stored`
      ].join(' • ');
    }

    if (verifyTargetEl) {
      verifyTargetEl.textContent = `${deviceName} • fw ${firmwareVersion} • schema ${schemaVersion}`;
    }

    if (exportConfigBtn) exportConfigBtn.disabled = !hasStagedConfig;
    if (exportLogBtn) exportLogBtn.disabled = sessionLogCount === 0;
    if (configBootBtn) {
      configBootBtn.disabled = simulator || Boolean(getConfigBootDisabled());
      configBootBtn.title = simulator
        ? 'Configurator reboot is unavailable while the simulator is active.'
        : 'Ask the current firmware image to reboot into configurator mode.';
    }
  }

  function open() {
    render();
    if (!dialog) return;
    if (typeof dialog.showModal === 'function') dialog.showModal();
    else dialog.setAttribute('open', '');
  }

  function close() {
    if (!dialog) return;
    if (typeof dialog.close === 'function') dialog.close();
    else dialog.removeAttribute('open');
  }

  function bind() {
    closeBtn?.addEventListener('click', () => close());
    dialog?.addEventListener('cancel', (event) => {
      event.preventDefault();
      close();
    });
    exportConfigBtn?.addEventListener('click', () => onExportConfig());
    exportLogBtn?.addEventListener('click', () => onExportSessionLog());
    configBootBtn?.addEventListener('click', () => onRequestConfiguratorBoot());
  }

  return {
    bind,
    close,
    open,
    render
  };
}
