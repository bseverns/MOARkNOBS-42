const PROFILE_SLOT_LABELS = ['A', 'B', 'C', 'D'];

function initializeMeters(container, count, labelPrefix) {
  if (!container) return [];
  container.innerHTML = '';
  const meters = [];
  for (let i = 0; i < count; i += 1) {
    const wrap = document.createElement('div');
    wrap.className = 'meter';
    const label = document.createElement('span');
    label.textContent = `${labelPrefix} ${String(i + 1).padStart(2, '0')}`;
    const progress = document.createElement('progress');
    progress.max = 127;
    progress.value = 0;
    const value = document.createElement('span');
    value.className = 'meter-value';
    value.textContent = '00';
    wrap.append(label, progress, value);
    container.appendChild(wrap);
    meters.push({ wrap, progress, value });
  }
  return meters;
}

export function createPerformerPanelController({
  elements = {},
  runtime,
  localManifest,
  resolveDeviceName,
  resolveFirmwareVersion,
  slotTypeAbbreviations = {},
  connect = () => {},
  getConnectionStage = () => 'disconnected',
  getConnectionText = () => 'Disconnected',
  getProfileText = () => 'Slot A - local target',
  getActiveProfileSlot = () => 0,
  setActiveProfileSlot = () => {},
  loadProfile = () => {},
  recallScene = () => {},
  setStatus = () => {}
} = {}) {
  const {
    panel = null,
    connectBtn = null,
    connectionState = null,
    dirtyState = null,
    deviceName = null,
    fwVersion = null,
    profileSummary = null,
    lastEvent = null,
    profileSelect = null,
    profileLoadBtn = null,
    sceneSelect = null,
    sceneRecallBtn = null,
    panicHelpBtn = null,
    slotGrid = null,
    envelopeContainer = null
  } = elements;

  let slotCells = [];
  let envMeters = [];

  function bind() {
    connectBtn?.addEventListener('click', () => connect());
    profileSelect?.addEventListener('change', () => {
      setActiveProfileSlot(Number(profileSelect.value));
      refresh();
    });
    profileLoadBtn?.addEventListener('click', () => loadProfile());
    sceneSelect?.addEventListener('change', () => refresh());
    sceneRecallBtn?.addEventListener('click', () => recallScene(Number(sceneSelect?.value ?? 0)));
    panicHelpBtn?.addEventListener('click', () => {
      setStatus(
        'warn',
        'Panic baseline',
        'Hardware combo only: press Ctrl0 + Ctrl1 + Ctrl2 to stop arp, disable EF follow, and reload the active profile baseline.'
      );
    });
  }

  function setVisible(visible) {
    panel?.toggleAttribute('hidden', !visible);
  }

  function rebuildMeters(count) {
    envMeters = initializeMeters(envelopeContainer, count, 'EF');
  }

  function renderSlots(slots) {
    if (!slotGrid) return;
    const source = Array.isArray(slots) ? slots : [];
    if (slotCells.length === source.length && slotCells.length) {
      slotCells.forEach((cell, index) => {
        const slot = source[index];
        const state = cell.querySelector('.stage-slot-type');
        if (state) state.textContent = slotTypeAbbreviations[slot?.type] ?? slot?.type ?? '-';
      });
      return;
    }

    slotGrid.innerHTML = '';
    slotCells = source.map((slot, index) => {
      const cell = document.createElement('div');
      cell.className = 'stage-slot-cell';
      cell.dataset.index = String(index);

      const label = document.createElement('span');
      label.className = 'stage-slot-label';
      label.textContent = String(index + 1).padStart(2, '0');

      const type = document.createElement('span');
      type.className = 'stage-slot-type';
      type.textContent = slotTypeAbbreviations[slot?.type] ?? slot?.type ?? '-';

      const value = document.createElement('span');
      value.className = 'stage-slot-value';
      value.textContent = '--';

      cell.append(label, type, value);
      slotGrid.appendChild(cell);
      return cell;
    });
  }

  function paintTelemetry(frame) {
    if (!Array.isArray(frame?.slots)) return;

    frame.envelopes?.forEach((value, idx) => {
      const entry = envMeters[idx];
      if (!entry) return;
      entry.progress.value = value;
      entry.value.textContent = String(value).padStart(2, '0');
    });

    frame.slots.forEach((value, idx) => {
      const cell = slotCells[idx];
      if (!cell) return;
      const numeric = Number(value);
      const active = Number.isFinite(numeric) && numeric > 0;
      cell.classList.toggle('active', active);
      const valueEl = cell.querySelector('.stage-slot-value');
      if (valueEl) valueEl.textContent = Number.isFinite(numeric) ? String(numeric) : '--';
    });

    if (lastEvent) {
      const currentSlot = Number.isFinite(Number(frame.currentSlot))
        ? `S${String(Number(frame.currentSlot) + 1).padStart(2, '0')}`
        : 'Telemetry';
      lastEvent.textContent = `${currentSlot} - ${new Date().toLocaleTimeString()}`;
    }
  }

  function refresh({
    profileLoadDisabled = true,
    sceneRecallDisabled = true,
    connected = false
  } = {}) {
    const manifest = runtime?.getState?.().manifest ?? localManifest;
    const dirtyNow = Boolean(runtime?.getState?.().dirty);

    if (connectionState) {
      connectionState.textContent = getConnectionText();
      connectionState.dataset.stage = getConnectionStage();
    }
    if (connectBtn) {
      connectBtn.textContent = connected ? 'Reconnect' : 'Connect';
    }
    if (dirtyState) {
      dirtyState.textContent = dirtyNow ? 'Dirty' : 'Clean';
      dirtyState.dataset.state = dirtyNow ? 'dirty' : 'clean';
    }
    if (deviceName) deviceName.textContent = resolveDeviceName?.(manifest) ?? '-';
    if (fwVersion) fwVersion.textContent = resolveFirmwareVersion?.(manifest) ?? 'unknown';
    if (profileSummary) profileSummary.textContent = getProfileText();
    if (profileSelect) {
      profileSelect.value = String(getActiveProfileSlot());
    }
    if (profileLoadBtn) profileLoadBtn.disabled = Boolean(profileLoadDisabled);
    if (sceneRecallBtn) sceneRecallBtn.disabled = Boolean(sceneRecallDisabled);
  }

  function slotLabel(index) {
    return PROFILE_SLOT_LABELS[index] ?? PROFILE_SLOT_LABELS[0];
  }

  return {
    bind,
    setVisible,
    rebuildMeters,
    renderSlots,
    paintTelemetry,
    refresh,
    slotLabel
  };
}
