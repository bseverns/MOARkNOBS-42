function formatCount(value, singular, plural = `${singular}s`) {
  const numeric = Number(value);
  if (!Number.isFinite(numeric) || numeric < 0) return `0 ${plural}`;
  return `${numeric} ${numeric === 1 ? singular : plural}`;
}

function formatBuildTime(value) {
  if (typeof value !== 'string' || !value.trim()) return 'Build time unavailable.';
  return value.trim();
}

export function createServicePanelController({
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
    verdict = null,
    verdictLabel = null,
    verdictMessage = null,
    connectionState = null,
    connectionDetail = null,
    firmwareState = null,
    firmwareDetail = null,
    dirtyState = null,
    dirtyDetail = null,
    backupState = null,
    backupDetail = null,
    logState = null,
    logDetail = null,
    verifyTarget = null,
    exportConfigBtn = null,
    exportLogBtn = null,
    configBootBtn = null
  } = elements;

  function bind() {
    exportConfigBtn?.addEventListener('click', () => onExportConfig());
    exportLogBtn?.addEventListener('click', () => onExportSessionLog());
    configBootBtn?.addEventListener('click', () => onRequestConfiguratorBoot());
  }

  function render() {
    const state = runtime?.getState?.() ?? {};
    const manifest = state.manifest ?? localManifest;
    const connectionStage = getConnectionStage();
    const connected = connectionStage === 'live';
    const dirty = Boolean(state.dirty);
    const hasStagedConfig = Boolean(state.staged);
    const simulator = Boolean(isSimulatorActive());
    const sessionLogCount = Number(getSessionLogCount()) || 0;
    const deviceName = resolveDeviceName(manifest);
    const firmwareVersion = resolveFirmwareVersion(manifest);
    const schemaVersion = manifest?.schema_version ?? localManifest?.schema_version ?? '?';
    const buildTime = formatBuildTime(manifest?.build_time);

    let verdictState = 'ok';
    let verdictHeading = 'Flash handoff ready';
    let verdictBody =
      'Export a backup, close other serial tools, flash the release or PlatformIO build, then reconnect and verify the handshake.';

    if (simulator) {
      verdictState = 'warn';
      verdictHeading = 'Simulator session only';
      verdictBody =
        'The Service tab can rehearse the update checklist, but the simulator cannot flash firmware or reboot a physical device.';
    } else if (dirty) {
      verdictState = 'warn';
      verdictHeading = 'Resolve staged edits first';
      verdictBody =
        'Apply or roll back the local diff before flashing so the browser state and device state do not drift apart.';
    } else if (!connected) {
      verdictState = 'warn';
      verdictHeading = 'No live device connected';
      verdictBody =
        'You can still review the flash lanes and export saved data, but reconnect after the update to confirm firmware identity and schema.';
    }

    if (verdict) verdict.dataset.state = verdictState;
    if (verdictLabel) verdictLabel.textContent = verdictHeading;
    if (verdictMessage) verdictMessage.textContent = verdictBody;

    if (connectionState) {
      connectionState.textContent = connected ? 'Connected' : 'Offline';
    }
    if (connectionDetail) {
      connectionDetail.textContent = connected
        ? `${deviceName} is on the wire now.`
        : 'No device handshake yet. Reconnect after the flash to confirm the new image.';
    }

    if (firmwareState) {
      firmwareState.textContent = `${firmwareVersion} • schema ${schemaVersion}`;
    }
    if (firmwareDetail) {
      firmwareDetail.textContent = connected
        ? `${deviceName} • ${buildTime}`
        : 'Firmware identity will refresh after the next successful handshake.';
    }

    if (dirtyState) {
      dirtyState.textContent = dirty ? 'Dirty' : hasStagedConfig ? 'Clean' : 'No config';
    }
    if (dirtyDetail) {
      dirtyDetail.textContent = dirty
        ? 'Local edits differ from the last verified device snapshot.'
        : hasStagedConfig
          ? 'Staged and live snapshots currently agree.'
          : 'Connect or import a config if you want a browser-side backup before flashing.';
    }

    if (backupState) {
      backupState.textContent = hasStagedConfig ? 'Ready' : 'Unavailable';
    }
    if (backupDetail) {
      backupDetail.textContent = hasStagedConfig
        ? 'Export config JSON now if you want a rollback file before touching firmware.'
        : 'No staged snapshot is loaded in the browser yet.';
    }

    if (logState) {
      logState.textContent = formatCount(sessionLogCount, 'entry');
    }
    if (logDetail) {
      logDetail.textContent =
        sessionLogCount > 0
          ? 'Export the session log if you want a preflight and recovery record.'
          : 'No session events captured yet.';
    }

    if (verifyTarget) {
      verifyTarget.textContent = `${deviceName} • fw ${firmwareVersion} • schema ${schemaVersion}`;
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

  return {
    bind,
    render
  };
}
