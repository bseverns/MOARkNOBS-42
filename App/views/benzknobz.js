import { createRuntime } from '../runtime.js';
import { FormRenderer } from './form_renderer.js';
import { MidiMonitor } from './midi_monitor.js';
import { presets } from './presets.js';
import { ScopePanel } from './scope_panel.js';

const EF_FILTER_NAMES = [
  'LINEAR',
  'OPPOSITE_LINEAR',
  'EXPONENTIAL',
  'RANDOM',
  'LOWPASS',
  'HIGHPASS',
  'BANDPASS'
];

const ARG_METHOD_NAMES = [
  'PLUS',
  'MIN',
  'PECK',
  'SHAV',
  'SQAR',
  'BABS',
  'TABS',
  'MULT',
  'DIVI',
  'AVG',
  'XABS',
  'MAXX',
  'MINN',
  'XORR'
];

const localManifest = {
  ui_version: '2025.03.01',
  schema_version: 5,
  slot_count: 42,
  pot_count: 42,
  envelope_count: 6,
  arg_method_count: ARG_METHOD_NAMES.length,
  led_count: 51
};

const runtimeOptions = {
  schemaUrl: './config_schema.json',
  localManifest,
  wsUrl:
    typeof window !== 'undefined' && typeof window.location === 'object'
      ? new URLSearchParams(window.location.search).get('ws') ?? undefined
      : undefined
};
if (
  typeof window !== 'undefined' &&
  window.__MN42_RUNTIME_OPTIONS &&
  typeof window.__MN42_RUNTIME_OPTIONS === 'object'
) {
  Object.assign(runtimeOptions, window.__MN42_RUNTIME_OPTIONS);
}
const runtime = createRuntime(runtimeOptions);
if (typeof window !== 'undefined') {
  window.__MN42_RUNTIME = runtime;
}

const sharedResizeObserver =
  typeof ResizeObserver === 'function'
    ? new ResizeObserver((entries) => {
        for (const entry of entries) {
          entry.target.__resizeCallback?.(entry);
        }
      })
    : null;

class VirtualGrid {
  constructor(container, { columns, rowHeight, render }) {
    this.container = container;
    this.columns = columns;
    this.rowHeight = rowHeight;
    this.renderItem = render;
    this.data = [];
    this.pool = [];
    this.viewport = document.createElement('div');
    this.viewport.className = 'virtual-grid';
    if (this.container) {
      this.container.style.position = 'relative';
      this.container.style.overflowY = 'auto';
      this.container.appendChild(this.viewport);
      this.container.__resizeCallback = () => this.compute();
      sharedResizeObserver?.observe(this.container);
    }
    this.viewport.style.position = 'relative';
    this.viewport.style.width = '100%';
    this.container?.addEventListener('scroll', () => this.render());
  }

  setData(data) {
    this.data = data || [];
    this.compute();
  }

  compute() {
    if (!this.container) return;
    const visibleRows = Math.ceil(this.container.clientHeight / this.rowHeight) + 2;
    const needed = visibleRows * this.columns;
    while (this.pool.length < needed) {
      const el = document.createElement('div');
      el.className = 'virtual-item';
      el.style.position = 'absolute';
      this.viewport.appendChild(el);
      this.pool.push(el);
    }
    this.render();
  }

  render() {
    if (!this.container) return;
    const scrollTop = this.container.scrollTop;
    const firstRow = Math.max(0, Math.floor(scrollTop / this.rowHeight) - 1);
    const startIndex = firstRow * this.columns;
    this.viewport.style.height = `${Math.ceil(this.data.length / this.columns) * this.rowHeight}px`;
    this.pool.forEach((el, idx) => {
      const dataIndex = startIndex + idx;
      if (dataIndex >= this.data.length) {
        el.style.display = 'none';
        return;
      }
      el.style.display = '';
      const row = Math.floor(dataIndex / this.columns);
      const column = dataIndex % this.columns;
      el.style.width = `${100 / this.columns}%`;
      el.style.transform = `translate(${column * 100}%, ${row * this.rowHeight}px)`;
      el.style.height = `${this.rowHeight}px`;
      el.dataset.index = String(dataIndex);
      this.renderItem(el, dataIndex, this.data[dataIndex]);
    });
  }

  scrollToIndex(index) {
    if (!this.container) return;
    const row = Math.floor(index / this.columns);
    const target = row * this.rowHeight;
    this.container.scrollTo({ top: target, behavior: 'smooth' });
  }

  highlight(index) {
    this.pool.forEach((el) => {
      el.classList.toggle('selected', Number(el.dataset.index) === index);
    });
  }

  updateTelemetry(values) {
    this.pool.forEach((el) => {
      if (el.dataset.index === undefined) return;
      const idx = Number(el.dataset.index);
      const value = values[idx] ?? 0;
      el.dataset.value = value;
      const meter = el.querySelector('.slot-value');
      if (meter) meter.textContent = value;
    });
  }
}

class VirtualList {
  constructor(container, { itemHeight, render }) {
    this.container = container;
    this.itemHeight = itemHeight;
    this.renderItem = render;
    this.data = [];
    this.pool = [];
    this.viewport = document.createElement('div');
    this.viewport.className = 'virtual-list';
    if (this.container) {
      this.container.style.position = 'relative';
      this.container.style.overflowY = 'auto';
      this.container.appendChild(this.viewport);
      this.container.__resizeCallback = () => this.compute();
      sharedResizeObserver?.observe(this.container);
    }
    this.viewport.style.position = 'relative';
    this.viewport.style.width = '100%';
    this.container?.addEventListener('scroll', () => this.render());
  }

  setData(data) {
    this.data = data || [];
    this.compute();
  }

  compute() {
    if (!this.container) return;
    const visible = Math.ceil(this.container.clientHeight / this.itemHeight) + 2;
    while (this.pool.length < visible) {
      const el = document.createElement('div');
      el.className = 'virtual-row';
      el.style.position = 'absolute';
      this.viewport.appendChild(el);
      this.pool.push(el);
    }
    this.render();
  }

  render() {
    if (!this.container) return;
    const scrollTop = this.container.scrollTop;
    const first = Math.max(0, Math.floor(scrollTop / this.itemHeight) - 1);
    this.viewport.style.height = `${this.data.length * this.itemHeight}px`;
    this.pool.forEach((el, idx) => {
      const dataIndex = first + idx;
      if (dataIndex >= this.data.length) {
        el.style.display = 'none';
        return;
      }
      el.style.display = '';
      el.style.width = '100%';
      el.style.transform = `translateY(${dataIndex * this.itemHeight}px)`;
      this.renderItem(el, dataIndex, this.data[dataIndex]);
    });
  }
}

const boot = () => {
  if (typeof document === 'undefined') return;
  const docRoot = document.documentElement;
  if (docRoot?.dataset?.mn42Booted === 'true') return;
  if (docRoot) docRoot.dataset.mn42Booted = 'true';
  const statusEl = document.getElementById('status');
  const statusLabel = document.getElementById('status-label');
  const statusMessage = statusEl?.querySelector('.status-message');
  const connectBtn = document.getElementById('connect');
  const applyBtn = document.getElementById('apply');
  const rollbackBtn = document.getElementById('rollback');
  const slotContainer = document.getElementById('slots');
  const envContainer = document.getElementById('envelopes');
  const diffPanel = document.getElementById('diff-panel');
  const diffOutput = document.getElementById('diff-output');
  const dirtyBadge = document.getElementById('dirty-badge');
  const connectionPill = document.getElementById('connection-pill');
  const headerStatus = document.getElementById('header-status');
  const exportPresetBtn = document.getElementById('export-preset');
  const importPresetBtn = document.getElementById('import-preset');
  const presetPicker = document.getElementById('preset-picker');
  const simulatorToggle = document.getElementById('simulator-toggle');
  const ledGrid = document.getElementById('led-grid');
  const formContainer = document.getElementById('form');
  const schemaSections = Array.from(document.querySelectorAll('[data-schema-target]')).map((element) => ({
    target: element,
    schemaPath: element.dataset.schemaTarget
  }));
  const formRenderer = new FormRenderer({ runtime, sections: schemaSections });
  const efAssignmentGrid = document.querySelector('#ef-assignment-card .ef-grid');
  const slotDetailIndex = document.getElementById('slot-detail-index');
  const slotDetailStatus = document.getElementById('slot-detail-status');
  const slotDetailType = document.getElementById('slot-detail-type');
  const slotDetailChannel = document.getElementById('slot-detail-channel');
  const slotDetailData = document.getElementById('slot-detail-data');
  const slotDetailEfIndex = document.getElementById('slot-detail-ef-index');
  const slotDetailEfFilter = document.getElementById('slot-detail-ef-filter');
  const slotDetailEfTuning = document.getElementById('slot-detail-ef-tuning');
  const slotDetailEfDynamics = document.getElementById('slot-detail-ef-dynamics');
  const slotDetailEfBaseline = document.getElementById('slot-detail-ef-baseline');
  const slotDetailArg = document.getElementById('slot-detail-arg');
  const slotDetailArgSources = document.getElementById('slot-detail-arg-sources');
  const slotDetailValue = document.getElementById('slot-detail-value');
  const deviceMonitor = document.getElementById('device-monitor');
  const logEl = document.getElementById('log');
  const profileSlotButtons = Array.from(document.querySelectorAll('[data-profile-slot]'));
  const profileSlotStatus = document.getElementById('profile-slot-status');
  const profileSaveBtn = document.getElementById('profile-save');
  const profileLoadBtn = document.getElementById('profile-load');
  const profileResetBtn = document.getElementById('profile-reset');
  const profileWizardTarget = document.getElementById('profile-wizard-target');
  const profileWizardSwitchBtn = document.getElementById('profile-wizard-switch');
  const profileWizardApplyBtn = document.getElementById('profile-wizard-apply');
  const profileWizardSaveBtn = document.getElementById('profile-wizard-save');
  const profileWizardStatus = document.getElementById('profile-wizard-status');
  const profileDownloadBtn = document.getElementById('profile-download');
  const profileUploadBtn = document.getElementById('profile-upload');
  const macroSaveBtn = document.getElementById('macro-save');
  const macroRecallBtn = document.getElementById('macro-recall');
  const macroStatusEl = document.getElementById('macro-status');
  const sceneGrid = document.getElementById('scene-grid');
  const sceneStatusEl = document.getElementById('scene-status');
  const uiModeButtons = Array.from(document.querySelectorAll('[data-ui-mode-btn]'));
  const uiModeHint = document.getElementById('ui-mode-hint');
  const advancedTierNodes = Array.from(document.querySelectorAll('[data-ui-tier="advanced"]'));
  const PROFILE_LABELS = ['A', 'B', 'C', 'D'];
  const MACRO_SAVE_COMMAND = 'SAVE_MACRO_SLOT';
  const MACRO_RECALL_COMMAND = 'RECALL_MACRO_SLOT';
  const PROFILE_STORAGE_KEY = 'moarknobs:selected-profile';
  const UI_MODE_STORAGE_KEY = 'moarknobs:ui-mode';
  const UI_MODE_HINTS = {
    basic: 'Basic mode keeps common knob-to-MIDI mapping controls visible.',
    advanced: 'Advanced mode reveals EF, ARG, filter tuning, and scope diagnostics.'
  };
  const GLOSSARY = {
    mapping: 'Knob to MIDI mapping: choose the message type, channel, and number your synth or DAW expects.',
    takeover: 'Take Control waits for the knob to pass the current value so tweaks do not jump.',
    ef: 'EF (Envelope Follower) tracks input level to drive dynamic modulation.',
    arg: 'ARG combines two envelope followers with a math method before mapping to MIDI.',
    filter: 'Filter shape and tuning control how aggressively the envelope follower reacts.',
    sysex: 'SysEx template uses hex bytes; XX, MSB, and LSB placeholders are replaced with live values.'
  };
  const migrationDialog = document.getElementById('migration-dialog');
  const profileRpcButtons = [profileSaveBtn, profileLoadBtn, profileResetBtn].filter(Boolean);
  let profileInteractable = false;
  let profileRpcLocked = false;
  let profileWizardBusy = false;
  let macroBusy = false;
  let macroAvailable = true;
  const SCENE_SLOT_COUNT = 6;
  const sceneSlotState = Array.from({ length: SCENE_SLOT_COUNT }, () => ({
    name: '',
    available: false
  }));
  const sceneSlotElements = [];
  let sceneBusy = false;
  let activeProfileSlot = clampProfileSlot(readProfileSlotPreference());
  let profileWizardTargetSlot = activeProfileSlot;
  let activeUiMode = normalizeUIMode(readUIModePreference());
  const migrationPreview = document.getElementById('migration-preview');
  const migrationApply = document.getElementById('migration-apply');
  const migrationCancel = document.getElementById('migration-cancel');
  const migrationExport = document.getElementById('migration-export');

  const slotState = {
    slots: [],
    efSlots: [],
    staged: null,
    selected: 0,
    telemetry: null
  };

  const slotVirtualizer = new VirtualGrid(slotContainer, {
    columns: 6,
    rowHeight: 100,
    render: renderSlotButton
  });

  const efVirtualizer = new VirtualList(efAssignmentGrid, {
    itemHeight: 48,
    render: renderEfRow
  });

  let envMeters = [];
  const rebuildMeters = (count) => {
    envMeters = initializeMeters(envContainer, count, 'EF');
  };
  rebuildMeters(localManifest.envelope_count || 0);
  slotVirtualizer.setData([]);

  if (sceneGrid) {
    for (let slotIndex = 0; slotIndex < SCENE_SLOT_COUNT; slotIndex++) {
      const slotEl = document.createElement('div');
      slotEl.className = 'scene-slot';
      slotEl.dataset.sceneSlot = String(slotIndex);
      slotEl.innerHTML = `
        <div class="scene-slot-meta">
          <span>Scene ${slotIndex + 1}</span>
          <span class="scene-slot-status">Slot ${slotIndex + 1} empty</span>
        </div>
        <input class="scene-name-input" type="text" maxlength="15" placeholder="Name (optional)" />
        <div class="scene-actions">
          <button class="scene-save" type="button" title="Save current state to this slot.">Save</button>
          <button class="scene-recall" type="button" title="Recall this scene snapshot.">Recall</button>
        </div>`;
      sceneGrid.appendChild(slotEl);
      const slotInfo = {
        slot: slotIndex,
        element: slotEl,
        statusEl: slotEl.querySelector('.scene-slot-status'),
        nameInput: slotEl.querySelector('.scene-name-input'),
        saveBtn: slotEl.querySelector('.scene-save'),
        recallBtn: slotEl.querySelector('.scene-recall')
      };
      slotInfo.saveBtn?.addEventListener('click', () => handleSceneSave(slotIndex));
      slotInfo.recallBtn?.addEventListener('click', () => handleSceneRecall(slotIndex));
      sceneSlotElements.push(slotInfo);
    }
  }

  connectBtn?.addEventListener('click', async () => {
    try {
      if (connectionPill) {
        connectionPill.dataset.stage = 'handshake';
        connectionPill.textContent = 'Handshaking…';
      }
      await runtime.connect();
    } catch (err) {
      if (connectionPill) {
        connectionPill.dataset.stage = 'disconnected';
        connectionPill.textContent = 'Disconnected';
      }
      setStatus('err', 'Connect failed', err.message || String(err));
    }
  });

  applyBtn?.addEventListener('click', async () => {
    try {
      setStatus('warn', 'Applying…', 'Waiting for firmware ACK');
      await runtime.apply();
      setStatus('ok', 'Synced', 'Device acknowledged the staged edits.');
    } catch (err) {
      setStatus('err', 'Apply failed', err.message || String(err));
    }
  });

  rollbackBtn?.addEventListener('click', async () => {
    await runtime.rollback();
    setStatus('warn', 'Rolled back', 'Local edits were discarded.');
  });

  if (simulatorToggle) {
    simulatorToggle.setAttribute('aria-pressed', simulatorToggle.classList.contains('active') ? 'true' : 'false');
  }

  if (simulatorToggle && !simulatorToggle.dataset.booted) {
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

  uiModeButtons.forEach((button) => {
    button.addEventListener('click', () => {
      setUIMode(button.dataset.uiModeBtn);
    });
  });
  setUIMode(activeUiMode, { persist: false });

  profileSlotButtons.forEach((button) => {
    button.addEventListener('click', () => {
      const slotIndex = clampProfileSlot(Number(button.dataset.profileSlot));
      setActiveProfileSlot(slotIndex);
    });
  });
  if (profileWizardTarget) {
    profileWizardTarget.value = String(profileWizardTargetSlot);
    profileWizardTarget.addEventListener('change', () => {
      profileWizardTargetSlot = clampProfileSlot(Number(profileWizardTarget.value));
      updateProfileWizardControls();
    });
  }
  profileWizardSwitchBtn?.addEventListener('click', () => handleWizardSwitchProfile());
  profileWizardApplyBtn?.addEventListener('click', () => handleWizardApplyEdits());
  profileWizardSaveBtn?.addEventListener('click', () => handleWizardSaveProfile());
  setActiveProfileSlot(activeProfileSlot, { persist: false });
  refreshProfileControls();

  macroSaveBtn?.addEventListener('click', () => {
    const confirmation =
      typeof window !== 'undefined' && typeof window.confirm === 'function'
        ? window.confirm('Overwrite the macro snapshot stored in EEPROM slot 254?')
        : true;
    if (!confirmation) return;
    runMacroCommand(MACRO_SAVE_COMMAND);
  });

  macroRecallBtn?.addEventListener('click', () => runMacroCommand(MACRO_RECALL_COMMAND));

  setMacroStatus('muted', 'Awaiting the first snapshot.');

  slotContainer?.addEventListener('keydown', (event) => {
    if (!slotState.slots.length) return;
    const { key } = event;
    const columns = slotVirtualizer.columns;
    let next = slotState.selected;
    if (key === 'ArrowRight') next += 1;
    else if (key === 'ArrowLeft') next -= 1;
    else if (key === 'ArrowUp') next -= columns;
    else if (key === 'ArrowDown') next += columns;
    else if (key === 'Home') next = 0;
    else if (key === 'End') next = slotState.slots.length - 1;
    else return;
    event.preventDefault();
    selectSlot(Math.max(0, Math.min(slotState.slots.length - 1, next)));
  });

  exportPresetBtn?.addEventListener('click', () => {
    const { staged } = runtime.getState();
    const blob = new Blob([JSON.stringify(staged, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = `moarknobz-preset-${new Date().toISOString().replace(/[:.]/g, '-')}.json`;
    anchor.click();
    URL.revokeObjectURL(url);
  });

  importPresetBtn?.addEventListener('click', () => {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json,application/json';
    input.onchange = async () => {
      const file = input.files?.[0];
      if (!file) return;
      const text = await file.text();
      try {
        const json = JSON.parse(text);
        runtime.stage(() => json);
      } catch (err) {
        setStatus('err', 'Import failed', err.message || String(err));
      }
    };
    input.click();
  });

  if (presetPicker) {
    const placeholder = presetPicker.querySelector('option[value=""]');
    if (!placeholder) {
      const option = document.createElement('option');
      option.value = '';
      option.textContent = 'Select a preset…';
      presetPicker.prepend(option);
    }

    const presetMap = new Map();
    presets.forEach((preset) => {
      if (presetMap.has(preset.id)) return;
      presetMap.set(preset.id, preset);
      const option = document.createElement('option');
      option.value = preset.id;
      option.textContent = preset.label;
      if (typeof preset.load !== 'function') {
        option.disabled = true;
      }
      presetPicker.appendChild(option);
    });
    presetPicker.selectedIndex = 0;

    presetPicker.addEventListener('change', async () => {
      const presetId = presetPicker.value;
      const descriptor = presetMap.get(presetId);
      if (!descriptor || typeof descriptor.load !== 'function') {
        presetPicker.selectedIndex = 0;
        return;
      }
      try {
        const config = await descriptor.load();
        if (!config || typeof config !== 'object') {
          throw new Error('Preset payload was empty');
        }
        runtime.stage(() => config);
        setStatus('warn', 'Preset staged', `${descriptor.label} staged. Hit Apply to push it to the deck.`);
      } catch (err) {
        setStatus('err', 'Preset load failed', err.message || String(err));
      } finally {
        presetPicker.selectedIndex = 0;
      }
    });
  }

  profileSaveBtn?.addEventListener('click', () =>
    runProfileRpc('save_profile', {
      busyLabel: 'Saving profile…',
      successLabel: 'Profile saved',
      successCopy: `${describeSlot()} archived`
    }),
  );
  profileLoadBtn?.addEventListener('click', () =>
    runProfileRpc('load_profile', {
      busyLabel: 'Switching profile…',
      successLabel: 'Profile switched',
      successCopy: `${describeSlot()} active`,
      expectConfig: true
    }),
  );
  profileResetBtn?.addEventListener('click', () =>
    runProfileRpc('reset_profile', {
      busyLabel: 'Resetting profile…',
      successLabel: 'Profile reset',
      successCopy: `${describeSlot()} restored`,
      expectConfig: true
    }),
  );
  profileDownloadBtn?.addEventListener('click', () => handleProfileDownload());
  profileUploadBtn?.addEventListener('click', () => handleProfileUpload());

  migrationApply?.addEventListener('click', () => migrationDialog?.close('apply'));
  migrationCancel?.addEventListener('click', () => migrationDialog?.close('cancel'));
  migrationExport?.addEventListener('click', () => exportPresetBtn?.click());
  migrationDialog?.addEventListener('close', () => {
    if (!migrationDialog?.returnValue || migrationDialog.returnValue === 'apply') {
      setStatus('warn', 'Migration ready', 'Review staged adapter before applying.');
    } else {
      setStatus('warn', 'Schema mismatch', 'Update firmware or UI so schemas agree.');
    }
  });

  runtime.on('status', ({ level, message }) => {
    // Normalize transport/runtime status levels onto the small UI state palette.
    const map = { error: 'err', warn: 'warn', info: 'ok', ok: 'ok' };
    const state = map[level] || 'warn';
    setStatus(state, level?.toUpperCase?.() || 'NOTE', message);
  });
  runtime.on('schema', (schema) => {
    formRenderer.updateSchema(schema);
  });
  runtime.on('telemetry', (frame) => {
    slotState.telemetry = frame;
    paintTelemetry(frame);
  });
  runtime.on('config', ({ staged, config, dirty }) => {
    // `staged` is the single source of truth for editor controls; keep all derived UI panes in
    // sync from this one event to avoid mixed snapshots.
    slotState.slots = staged?.slots ?? [];
    slotState.efSlots = staged?.efSlots ?? [];
    slotState.staged = staged;
    if (slotState.selected >= slotState.slots.length) {
      slotState.selected = Math.max(0, slotState.slots.length - 1);
    }
    updateHeader(config);
    slotVirtualizer.setData(slotState.slots);
    slotVirtualizer.highlight(slotState.selected);
    efVirtualizer.setData(slotState.efSlots);
    updateDiff(dirty);
    markDirty(dirty);
    populateDetail();
    renderLedControls(staged);
    updateTakeoverGuards(slotState.slots);
    formRenderer.updateValues();
    updateProfileWizardControls();
  });
  runtime.on('manifest', (manifest) => {
    updateHeaderManifest(manifest);
    renderDeviceMonitor(manifest);
    const followerCount = Number.isFinite(Number(manifest?.envelope_count))
      ? Number(manifest.envelope_count)
      : localManifest.envelope_count || 0;
    rebuildMeters(followerCount);
  });
  runtime.on('log', (line) => {
    if (!logEl) return;
    logEl.textContent += `${line}\n`;
  });
  runtime.on('validation-error', (errors) => {
    diffPanel?.removeAttribute('hidden');
    diffOutput.textContent = `Schema violations:\n${errors.map((e) => `• ${e.instancePath || '/'} ${e.message}`).join('\n')}`;
  });
  runtime.on('applied', ({ checksum }) => {
    diffPanel?.setAttribute('hidden', '');
    diffOutput.textContent = '';
    setStatus('ok', 'Device synced', `Checksum ${checksum.slice(0, 8)}…`);
  });
  runtime.on('migration-required', ({ from, to, canAdapt }) => {
    if (!migrationDialog || !migrationPreview) return;
    migrationPreview.textContent = `Firmware schema ${from} vs UI ${to}. ${canAdapt ? 'An adapter is available; stage edits then apply.' : 'Export your preset and update firmware/UI to continue.'}`;
    migrationDialog.showModal();
  });
  runtime.on('connected', ({ manifest }) => {
    // Connection flips the entire toolbar/profile surface into interactive mode.
    if (connectionPill) {
      connectionPill.dataset.stage = 'live';
      connectionPill.textContent = `Connected • ${manifest?.fw_version || 'fw?'}`;
    }
    if (applyBtn) applyBtn.disabled = true;
    if (rollbackBtn) rollbackBtn.disabled = true;
    if (exportPresetBtn) exportPresetBtn.disabled = false;
    if (importPresetBtn) importPresetBtn.disabled = false;
    setStatus('ok', 'Connected', 'Schema synced. Stage edits before applying.');
    profileInteractable = true;
    refreshProfileControls();
    refreshSceneList();
  });
  runtime.on('disconnected', () => {
    // Mirror the connected handler in reverse so stale controls cannot issue RPCs offline.
    if (connectionPill) {
      connectionPill.dataset.stage = 'disconnected';
      connectionPill.textContent = 'Disconnected';
    }
    if (applyBtn) applyBtn.disabled = true;
    if (rollbackBtn) rollbackBtn.disabled = true;
    if (exportPresetBtn) exportPresetBtn.disabled = true;
    if (importPresetBtn) importPresetBtn.disabled = true;
    setStatus('warn', 'Disconnected', 'Reconnect to continue editing.');
    profileInteractable = false;
    profileRpcLocked = false;
    refreshProfileControls();
    setSceneStatus('muted', 'Scenes offline');
  });
  runtime.on('error', (err) => {
    // Runtime errors are treated as hard disconnects from the UI perspective.
    if (connectionPill) {
      connectionPill.dataset.stage = 'disconnected';
      connectionPill.textContent = 'Disconnected';
    }
    setStatus('err', 'Runtime error', err.message || String(err));
    profileInteractable = false;
    profileRpcLocked = false;
    refreshProfileControls();
    setSceneStatus('muted', 'Scenes offline');
  });
  runtime.on('macro', ({ available } = {}) => {
    if (available === undefined) return;
    macroAvailable = Boolean(available);
    updateMacroControls();
  });
  runtime.on('scene', (payload) => {
    if (!sceneSlotElements.length || !payload) return;
    if (payload.type === 'list' && Array.isArray(payload.scenes)) {
      payload.scenes.forEach((entry) => {
        if (typeof entry.slot === 'number') {
          updateSceneSlot(entry.slot, {
            name: entry.name ?? '',
            available: entry.available
          });
        }
      });
      setSceneStatus('muted', 'Scenes synced with the deck.');
      return;
    }
    if (payload.type === 'saved' && typeof payload.slot === 'number') {
      updateSceneSlot(payload.slot, {
        name: payload.name ?? '',
        available: payload.available
      });
    }
    if (payload.type === 'recalled' && typeof payload.slot === 'number') {
      updateSceneSlot(payload.slot, {
        name: payload.name ?? '',
        available: payload.available
      });
    }
  });
  runtime.on('rollback', () => {
    updateDiff(false);
    markDirty(false);
    setStatus('warn', 'Rollback', 'Local edits were discarded.');
  });

  runtime.restoreLocalState();
  new MidiMonitor({ container: document.getElementById('midi-panel') });
  new ScopePanel({ container: document.getElementById('scope-panel'), runtime, manifest: localManifest });

  function setStatus(state, label, message) {
    // Single status sink used by transport, schema, profile, macro, and scene flows.
    if (!statusEl || !statusLabel || !statusMessage) return;
    statusEl.dataset.state = state;
    statusLabel.textContent = label;
    statusMessage.textContent = message;
  }

  function clampProfileSlot(value) {
    const idx = Number(value);
    if (!Number.isFinite(idx)) return 0;
    return Math.max(0, Math.min(PROFILE_LABELS.length - 1, Math.floor(idx)));
  }

  function readProfileSlotPreference() {
    if (typeof localStorage === 'undefined') return 0;
    try {
      const raw = localStorage.getItem(PROFILE_STORAGE_KEY);
      const parsed = Number(raw);
      if (Number.isFinite(parsed) && parsed >= 0 && parsed < PROFILE_LABELS.length) {
        return parsed;
      }
    } catch (err) {
      console.debug('read profile slot preference failed', err);
    }
    return 0;
  }

  function persistProfileSlot(index) {
    if (typeof localStorage === 'undefined') return;
    try {
      localStorage.setItem(PROFILE_STORAGE_KEY, String(index));
    } catch (err) {
      console.debug('persist profile slot failed', err);
    }
  }

  function normalizeUIMode(mode) {
    return mode === 'advanced' ? 'advanced' : 'basic';
  }

  function readUIModePreference() {
    if (typeof localStorage === 'undefined') return 'basic';
    try {
      return normalizeUIMode(localStorage.getItem(UI_MODE_STORAGE_KEY));
    } catch (err) {
      console.debug('read ui mode preference failed', err);
      return 'basic';
    }
  }

  function persistUIMode(mode) {
    if (typeof localStorage === 'undefined') return;
    try {
      localStorage.setItem(UI_MODE_STORAGE_KEY, normalizeUIMode(mode));
    } catch (err) {
      console.debug('persist ui mode failed', err);
    }
  }

  function setUIMode(mode, { persist = true } = {}) {
    activeUiMode = normalizeUIMode(mode);
    if (docRoot) docRoot.dataset.uiMode = activeUiMode;
    // Basic mode is presentational only: it hides advanced cards but preserves full runtime state.
    const hideAdvanced = activeUiMode !== 'advanced';
    advancedTierNodes.forEach((node) => {
      node.classList.toggle('ui-tier-hidden', hideAdvanced);
      if (hideAdvanced) {
        node.setAttribute('aria-hidden', 'true');
      } else {
        node.removeAttribute('aria-hidden');
      }
    });
    uiModeButtons.forEach((button) => {
      const buttonMode = normalizeUIMode(button.dataset.uiModeBtn);
      button.setAttribute('aria-pressed', buttonMode === activeUiMode ? 'true' : 'false');
    });
    if (uiModeHint) {
      uiModeHint.textContent = UI_MODE_HINTS[activeUiMode] || UI_MODE_HINTS.basic;
    }
    if (persist) persistUIMode(activeUiMode);
    if (slotState.slots.length) renderSlotEditor();
  }

  function slotLabel(index) {
    return PROFILE_LABELS[index] ?? PROFILE_LABELS[0];
  }

  function describeSlot(index = activeProfileSlot) {
    return `Slot ${slotLabel(index)}`;
  }

  function setActiveProfileSlot(index, { persist = true } = {}) {
    const bounded = clampProfileSlot(index);
    activeProfileSlot = bounded;
    profileSlotButtons.forEach((button) => {
      const slotValue = Number(button.dataset.profileSlot);
      if (!Number.isFinite(slotValue)) return;
      button.setAttribute('aria-pressed', slotValue === bounded ? 'true' : 'false');
    });
    if (profileSlotStatus) {
      profileSlotStatus.textContent = `${describeSlot(bounded)} • mirrored`;
    }
    if (persist) {
      persistProfileSlot(bounded);
    }
  }

  function refreshProfileControls() {
    const canInteract = profileInteractable && !profileRpcLocked;
    profileRpcButtons.forEach((button) => {
      if (!button) return;
      button.disabled = !canInteract;
    });
    updateProfileWizardControls();
    updateMacroControls();
    updateSceneControls();
  }

  function setProfileWizardStatus(state, message) {
    if (!profileWizardStatus) return;
    profileWizardStatus.dataset.state = state;
    profileWizardStatus.textContent = message;
  }

  function updateProfileWizardControls() {
    // Guided flow state machine: switch to target slot -> apply dirty edits -> save slot.
    const target = clampProfileSlot(Number(profileWizardTarget?.value ?? profileWizardTargetSlot));
    profileWizardTargetSlot = target;
    const canInteract = profileInteractable && !profileRpcLocked && !profileWizardBusy;
    if (profileWizardTarget) profileWizardTarget.disabled = !canInteract;
    if (profileWizardSwitchBtn) profileWizardSwitchBtn.disabled = !canInteract;
    const dirtyNow = runtime.getState().dirty;
    const onTarget = activeProfileSlot === target;
    if (profileWizardApplyBtn) {
      profileWizardApplyBtn.disabled = !canInteract || !onTarget || !dirtyNow;
    }
    if (profileWizardSaveBtn) {
      profileWizardSaveBtn.disabled = !canInteract || !onTarget;
    }

    if (!profileInteractable) {
      setProfileWizardStatus('muted', 'Connect to the device to start the guided flow.');
      return;
    }
    if (!onTarget) {
      setProfileWizardStatus(
        'busy',
        `Step 1: switch to Slot ${slotLabel(target)} (currently ${slotLabel(activeProfileSlot)}).`,
      );
      return;
    }
    if (dirtyNow) {
      setProfileWizardStatus('busy', `Step 2: apply edits, then save Slot ${slotLabel(target)}.`);
      return;
    }
    setProfileWizardStatus('ok', `Step 3: save Slot ${slotLabel(target)} to store current mapping.`);
  }

  function updateMacroControls() {
    const offline = !profileInteractable;
    if (macroSaveBtn) macroSaveBtn.disabled = offline || macroBusy;
    if (macroRecallBtn) macroRecallBtn.disabled = offline || macroBusy || !macroAvailable;
  }

  function setMacroStatus(state, message) {
    if (!macroStatusEl) return;
    macroStatusEl.dataset.state = state;
    if (typeof message === 'string') {
      macroStatusEl.textContent = message;
    }
  }

  function setSceneStatus(state, message) {
    if (!sceneStatusEl) return;
    sceneStatusEl.dataset.state = state;
    if (typeof message === 'string') {
      sceneStatusEl.textContent = message;
    }
  }

  function updateSceneSlot(slotIndex, { name, available }) {
    const slotInfo = sceneSlotElements.find((entry) => entry.slot === slotIndex);
    if (!slotInfo) return;
    const displayName = available
      ? name || `Scene ${slotIndex + 1}`
      : `Slot ${slotIndex + 1} empty`;
    if (slotInfo.statusEl) slotInfo.statusEl.textContent = displayName;
    sceneSlotState[slotIndex] = { name: name ?? '', available: Boolean(available) };
    updateSceneControls();
  }

  function updateSceneControls() {
    const offline = !profileInteractable;
    sceneSlotElements.forEach((slotInfo) => {
      const state = sceneSlotState[slotInfo.slot];
      if (slotInfo.saveBtn) slotInfo.saveBtn.disabled = offline || sceneBusy;
      if (slotInfo.recallBtn) {
        slotInfo.recallBtn.disabled = offline || sceneBusy || !state.available;
      }
    });
  }

  async function refreshSceneList() {
    if (!sceneGrid) return;
    setSceneStatus('busy', 'Loading scenes…');
    try {
      await runtime.requestScenes();
      setSceneStatus('muted', 'Scenes synced with the deck.');
    } catch (err) {
      setSceneStatus('err', `Scenes refresh failed: ${err.message || String(err)}`);
    }
  }

  async function handleSceneSave(slotIndex) {
    if (!profileInteractable || sceneBusy) return;
    const slotInfo = sceneSlotElements.find((entry) => entry.slot === slotIndex);
    if (!slotInfo) return;
    const name = slotInfo.nameInput?.value.trim();
    sceneBusy = true;
    updateSceneControls();
    setSceneStatus('busy', `Saving scene ${slotIndex + 1}…`);
    try {
      await runtime.sendSceneCommand({
        cmd: 'SAVE_SCENE',
        slot: slotIndex,
        name: name || undefined
      });
      setSceneStatus('ok', `Scene ${slotIndex + 1} saved.`);
      await refreshSceneList();
    } catch (err) {
      setSceneStatus('err', `Scene save failed: ${err.message || String(err)}`);
    } finally {
      sceneBusy = false;
      updateSceneControls();
    }
  }

  async function handleSceneRecall(slotIndex) {
    if (!profileInteractable || sceneBusy) return;
    sceneBusy = true;
    updateSceneControls();
    setSceneStatus('busy', `Recalling scene ${slotIndex + 1}…`);
    try {
      await runtime.sendSceneCommand({ cmd: 'RECALL_SCENE', slot: slotIndex });
      setSceneStatus('ok', `Scene ${slotIndex + 1} recalled.`);
      try {
        const configPayload = await runtime.sendRpc({ rpc: 'get_config' });
        const configData = configPayload?.config ?? configPayload;
        if (configData && typeof configData === 'object') {
          runtime.replaceConfig(configData);
        }
      } catch (refreshErr) {
        setStatus('warn', 'Config refresh failed', refreshErr.message || String(refreshErr));
      }
      await refreshSceneList();
    } catch (err) {
      setSceneStatus('err', `Scene recall failed: ${err.message || String(err)}`);
    } finally {
      sceneBusy = false;
      updateSceneControls();
    }
  }

  async function runMacroCommand(command) {
    if (!profileInteractable) {
      setMacroStatus('muted', 'Connect to the deck before using macro snapshots.');
      setStatus('warn', 'Macro offline', 'Connect to the deck before using macro snapshots.');
      return;
    }
    macroBusy = true;
    updateMacroControls();
    const isSave = command === MACRO_SAVE_COMMAND;
    setMacroStatus('busy', isSave ? 'Saving macro snapshot…' : 'Recalling macro snapshot…');
    try {
      await runtime.sendMacroCommand(command);
      setMacroStatus(
        'ok',
        isSave ? 'Macro snapshot stored in EEPROM slot 254.' : 'Macro snapshot recalled from EEPROM slot 254.'
      );
      if (!isSave) {
        try {
          const configPayload = await runtime.sendRpc({ rpc: 'get_config' });
          const configData = configPayload?.config ?? configPayload;
          if (configData && typeof configData === 'object') {
            runtime.replaceConfig(configData);
          }
        } catch (refreshErr) {
          setStatus('warn', 'Config refresh failed', refreshErr.message || String(refreshErr));
        }
      }
    } catch (err) {
      const label = isSave ? 'Macro save' : 'Macro recall';
      setMacroStatus('err', `${label} failed: ${err.message || String(err)}`);
      setStatus('err', `${label} failed`, err.message || String(err));
    } finally {
      macroBusy = false;
      updateMacroControls();
    }
  }

  async function runProfileRpc(method, { busyLabel, successLabel, successCopy, expectConfig } = {}) {
    // Shared profile RPC lane: one in-flight action at a time to keep slot/apply state coherent.
    if (!profileInteractable) {
      setStatus('warn', 'Profile offline', 'Connect to the deck before using profiles.');
      return;
    }
    const dirtyBefore = runtime.getState().dirty;
    profileRpcLocked = true;
    refreshProfileControls();
    try {
      // Saving a profile always snapshots firmware state, so stage/apply first to avoid storing
      // stale EEPROM values when local edits are still dirty.
      if (method === 'save_profile' && dirtyBefore) {
        setStatus('warn', 'Applying staged edits…', `${describeSlot()} • syncing before save`);
        await runtime.apply();
      }
    } catch (err) {
      profileRpcLocked = false;
      refreshProfileControls();
      setStatus('err', 'Apply failed', err.message || String(err));
      return;
    }
    setStatus('warn', busyLabel, `${describeSlot()} • busy`);
    try {
      const response = await runtime.sendRpc({ rpc: method, slot: activeProfileSlot });
      // Firmware may report an authoritative slot index; trust it and realign local selection.
      const responseSlot = clampProfileSlot(response?.slot ?? response?.profile ?? activeProfileSlot);
      setActiveProfileSlot(responseSlot);
      if (expectConfig) {
        const payload = response?.config ?? response;
        if (payload && typeof payload === 'object') {
          runtime.replaceConfig(payload);
        }
      }
      setStatus('ok', successLabel, successCopy ?? describeSlot());
    } catch (err) {
      setStatus('err', `${successLabel ?? 'Profile'} failed`, err.message || String(err));
    } finally {
      profileRpcLocked = false;
      refreshProfileControls();
    }
  }

  async function handleWizardSwitchProfile() {
    // Step 1: bind the UI to the selected slot and pull that slot's config into staged/live state.
    if (!profileInteractable || profileRpcLocked || profileWizardBusy) return;
    profileWizardBusy = true;
    refreshProfileControls();
    try {
      setActiveProfileSlot(profileWizardTargetSlot);
      await runProfileRpc('load_profile', {
        busyLabel: 'Switching profile…',
        successLabel: 'Profile switched',
        successCopy: `${describeSlot()} active`,
        expectConfig: true,
      });
    } finally {
      profileWizardBusy = false;
      refreshProfileControls();
    }
  }

  async function handleWizardApplyEdits() {
    // Step 2: commit staged edits to firmware (without persisting profile slot yet).
    if (!profileInteractable || profileRpcLocked || profileWizardBusy) return;
    if (activeProfileSlot !== profileWizardTargetSlot) {
      setStatus('warn', 'Wrong slot', `Switch to Slot ${slotLabel(profileWizardTargetSlot)} first.`);
      return;
    }
    if (!runtime.getState().dirty) {
      setStatus('warn', 'Nothing to apply', 'Adjust a parameter before applying.');
      return;
    }
    profileWizardBusy = true;
    refreshProfileControls();
    try {
      setStatus('warn', 'Applying…', `${describeSlot()} • waiting for firmware ACK`);
      await runtime.apply();
      setStatus('ok', 'Synced', `${describeSlot()} applied. Now save to persist.`);
    } catch (err) {
      setStatus('err', 'Apply failed', err.message || String(err));
    } finally {
      profileWizardBusy = false;
      refreshProfileControls();
    }
  }

  async function handleWizardSaveProfile() {
    // Step 3: persist the currently active slot after Step 2 succeeds.
    if (!profileInteractable || profileRpcLocked || profileWizardBusy) return;
    if (activeProfileSlot !== profileWizardTargetSlot) {
      setStatus('warn', 'Wrong slot', `Switch to Slot ${slotLabel(profileWizardTargetSlot)} first.`);
      return;
    }
    profileWizardBusy = true;
    refreshProfileControls();
    try {
      await runProfileRpc('save_profile', {
        busyLabel: 'Saving profile…',
        successLabel: 'Profile saved',
        successCopy: `${describeSlot()} archived`,
      });
    } finally {
      profileWizardBusy = false;
      refreshProfileControls();
    }
  }

  function handleProfileDownload() {
    const { staged, live } = runtime.getState();
    const payload = staged ?? live;
    if (!payload || typeof payload !== 'object') {
      setStatus('warn', 'Nothing to download', 'Stage a profile before exporting.');
      return;
    }
    const configPayload = {
      slot: activeProfileSlot,
      schema_version: runtime.getState().schema?.schema_version,
      timestamp: new Date().toISOString(),
      config: payload
    };
    const blob = new Blob([JSON.stringify(configPayload, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = `moarknobz-profile-${slotLabel(activeProfileSlot)}-${new Date().toISOString().replace(/[:.]/g, '-')}.json`;
    anchor.click();
    URL.revokeObjectURL(url);
    setStatus('ok', 'Profile downloaded', `${describeSlot()} saved locally.`);
  }

  function handleProfileUpload() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json,application/json';
    input.onchange = async () => {
      const file = input.files?.[0];
      if (!file) return;
      try {
        const text = await file.text();
        const documentPayload = JSON.parse(text);
        const configData = documentPayload?.config ?? documentPayload?.profile ?? documentPayload;
        if (!configData || typeof configData !== 'object') {
          throw new Error('File did not contain a config payload');
        }
        const requestedSlot = clampProfileSlot(documentPayload?.slot ?? documentPayload?.profile_slot ?? documentPayload?.slotIndex);
        if (Number.isFinite(requestedSlot)) {
          setActiveProfileSlot(requestedSlot);
        }
        // Import is staged-only by design so users can inspect diffs before pushing to hardware.
        runtime.stage(() => configData);
        setStatus('warn', 'Profile imported', `${describeSlot()} staged. Apply to push it to the deck.`);
      } catch (err) {
        setStatus('err', 'Profile import failed', err.message || String(err));
      }
    };
    input.click();
  }

  function markDirty(isDirty) {
    console.debug('[UI] markDirty', isDirty);
    if (dirtyBadge) dirtyBadge.toggleAttribute('hidden', !isDirty);
    if (applyBtn) applyBtn.disabled = !isDirty;
    if (rollbackBtn) rollbackBtn.disabled = !isDirty;
  }

  function updateDiff(isDirty) {
    if (!diffPanel || !diffOutput) return;
    const changes = runtime.diff();
    if (!isDirty || !changes.length) {
      diffPanel.setAttribute('hidden', '');
      diffOutput.textContent = '';
      return;
    }
    diffPanel.removeAttribute('hidden');
    const maxVisibleChanges = 40;
    const visibleChanges = changes.slice(0, maxVisibleChanges);
    const lines = visibleChanges.map(({ path, before, after }) => {
      const beforeText = summarizeDiffValue(before);
      const afterText = summarizeDiffValue(after);
      return `• ${path}\n  before: ${beforeText}\n  after:  ${afterText}`;
    });
    if (changes.length > maxVisibleChanges) {
      lines.push(`… ${changes.length - maxVisibleChanges} additional change(s) hidden.`);
    }
    const title = `${changes.length} staged change${changes.length === 1 ? '' : 's'}`;
    diffOutput.textContent = `${title}\n\n${lines.join('\n\n')}`;
  }

  function summarizeDiffValue(value) {
    if (value === undefined) return 'undefined';
    if (value === null) return 'null';
    if (typeof value === 'string') return truncateDiffText(JSON.stringify(value), 150);
    if (typeof value === 'number' || typeof value === 'boolean') return String(value);
    if (Array.isArray(value)) {
      return `Array(${value.length}) ${truncateDiffText(stringifyDiffValue(value), 150)}`;
    }
    if (typeof value === 'object') {
      const keys = Object.keys(value);
      const preview = keys.slice(0, 5).join(', ');
      const suffix = keys.length > 5 ? ', …' : '';
      return `Object{${preview}${suffix}} ${truncateDiffText(stringifyDiffValue(value), 140)}`;
    }
    return truncateDiffText(String(value), 150);
  }

  function stringifyDiffValue(value) {
    try {
      return JSON.stringify(value);
    } catch {
      return '[unserializable]';
    }
  }

  function truncateDiffText(text, maxLength) {
    if (typeof text !== 'string') return '';
    if (text.length <= maxLength) return text;
    return `${text.slice(0, Math.max(0, maxLength - 1))}…`;
  }

  function updateHeader(config) {
    if (!headerStatus) return;
    const manifest = runtime.getState().manifest;
    const ramBytes = Number(manifest?.free_ram);
    const flashBytes = Number(manifest?.free_flash);
    const ram = Number.isFinite(ramBytes) ? `${Math.round(ramBytes / 1024)}k RAM` : 'ram?';
    const flash = Number.isFinite(flashBytes) ? `${Math.round(flashBytes / 1024)}k flash` : 'flash?';
    headerStatus.textContent = [manifest?.fw_version || 'fw?', manifest?.schema_version ?? 'schema?', `${ram} • ${flash}`].join(' • ');
  }

  function updateHeaderManifest(manifest) {
    if (!connectionPill) return;
    connectionPill.dataset.stage = 'live';
    connectionPill.textContent = `Connected • ${manifest.fw_version || 'fw?'}`;
    updateHeader(runtime.getState().live);
  }

  function selectSlot(index) {
    const maxIndex = Math.max(0, slotState.slots.length - 1);
    slotState.selected = Math.min(Math.max(0, index), maxIndex);
    slotVirtualizer.highlight(slotState.selected);
    slotVirtualizer.scrollToIndex(slotState.selected);
    populateDetail();
  }

  function populateDetail() {
    const slot = slotState.slots[slotState.selected];
    const telemetry = slotState.telemetry || {};
    if (slotDetailIndex) slotDetailIndex.textContent = slotState.selected !== undefined ? `Slot ${String(slotState.selected + 1).padStart(2, '0')}` : '—';
    if (slotDetailStatus) slotDetailStatus.textContent = slot?.active ? 'Active' : 'Muted';
    if (slotDetailType) slotDetailType.textContent = slot?.type ?? '—';
    if (slotDetailChannel) slotDetailChannel.textContent = slot?.midiChannel ?? '—';
    if (slotDetailData) {
      if (slot?.type === 'SysEx') {
        slotDetailData.textContent = slot?.sysexTemplate && slot.sysexTemplate.length ? slot.sysexTemplate : '—';
      } else {
        slotDetailData.textContent = slot?.data1 ?? '—';
      }
    }
    const ef = normalizeEf(slot);
    if (slotDetailEfIndex) slotDetailEfIndex.textContent = formatEfIndex(ef.index);
    if (slotDetailEfFilter) slotDetailEfFilter.textContent = formatEfFilter(ef);
    if (slotDetailEfTuning) slotDetailEfTuning.textContent = formatEfTuning(ef);
    if (slotDetailEfDynamics) slotDetailEfDynamics.textContent = formatEfDynamics(ef);
    if (slotDetailEfBaseline) slotDetailEfBaseline.textContent = formatEfBaseline(ef);
    const arg = normalizeArg(slot);
    if (slotDetailArg) slotDetailArg.textContent = formatArgMode(arg);
    if (slotDetailArgSources) slotDetailArgSources.textContent = formatArgSources(arg);
    const value = Array.isArray(telemetry.slots) ? telemetry.slots[slotState.selected] : null;
    if (slotDetailValue) slotDetailValue.textContent = value ?? '—';
    renderSlotEditor();
  }

  function renderSlotEditor() {
    if (!formContainer) return;
    formContainer.innerHTML = '';
    const slot = slotState.slots[slotState.selected];
    if (!slot) {
      formContainer.textContent = 'Select a slot to edit.';
      return;
    }
    const form = document.createElement('form');
    form.className = 'slot-editor';
    form.addEventListener('submit', (event) => event.preventDefault());

    const basics = makeFieldset(
      'Knob -> MIDI Mapping',
      'Pick what this knob sends. Switch to Advanced mode for EF/ARG modulation and deep filter controls.',
    );
    basics.appendChild(
      makeSelect(
        'Knob -> MIDI message',
        ['OFF', 'CC', 'Note', 'PitchBend', 'ProgramChange', 'Aftertouch', 'ModWheel', 'NRPN', 'RPN', 'SysEx'],
        slot.type,
        (value) => stageSlotField(slotState.selected, 'type', value),
        { help: GLOSSARY.mapping },
      ),
    );
    basics.appendChild(
      makeNumber(
        'MIDI channel',
        slot.midiChannel ?? 1,
        1,
        16,
        1,
        (value) => stageSlotField(slotState.selected, 'midiChannel', value),
        { help: 'Use channels 1 to 16 to match your synth or DAW track.' },
      ),
    );
    basics.appendChild(
      makeNumber(
        'CC/Note number',
        slot.data1 ?? 0,
        0,
        127,
        1,
        (value) => stageSlotField(slotState.selected, 'data1', value),
        { help: 'The controller number or note number this knob targets.' },
      ),
    );
    basics.appendChild(
      makeText('Slot label', slot.label ?? '', 'Verse / build / drop cues', (value) => stageSlotField(slotState.selected, 'label', value)),
    );
    if (activeUiMode === 'advanced') {
      basics.appendChild(
        makeNumber('Arp root note', slot.arpNote ?? 0, 0, 127, 1, (value) => stageSlotField(slotState.selected, 'arpNote', value)),
      );
    }
    basics.appendChild(
      makeToggle('Enabled', !!slot.active, (value) => stageSlotField(slotState.selected, 'active', value)),
    );
    basics.appendChild(
      makeToggle('Knob sends MIDI', !!slot.pot, (value) => stageSlotField(slotState.selected, 'pot', value)),
    );
    const takeover = makeToggle('Take Control (pickup)', !!slot.takeover, (value) => {
      stageSlotField(slotState.selected, 'takeover', value);
      runtime.setPotGuard([slotState.selected], !value);
    }, { help: GLOSSARY.takeover });
    basics.appendChild(takeover);
    if (slot.type === 'SysEx') {
      basics.appendChild(
        makeText(
          'SysEx Template',
          slot.sysexTemplate ?? '',
          'F0 7F 01 04 XX F7',
          (value) => {
            const normalised = normaliseSysexTemplate(value);
            stageSlotField(slotState.selected, 'sysexTemplate', normalised);
          },
          { help: GLOSSARY.sysex },
        ),
      );
      const hint = document.createElement('p');
      hint.className = 'slot-hint';
      hint.textContent = 'Hex bytes + XX/MSB/LSB placeholders. We swap the placeholders with live values.';
      basics.appendChild(hint);
    }
    if (activeUiMode !== 'advanced') {
      const hint = document.createElement('p');
      hint.className = 'slot-hint';
      hint.textContent = 'Need EF or ARG modulation? Switch to Advanced mode.';
      basics.appendChild(hint);
    }
    form.appendChild(basics);

    if (activeUiMode === 'advanced') {
      const manifest = runtime.getState().manifest ?? localManifest;
      const ef = normalizeEf(slot);
      const efSlots = manifest?.envelope_count ?? localManifest?.envelope_count ?? 0;
      const efFieldset = makeFieldset(
        'Envelope Follower (EF)',
        'EF tracks input level so this slot can react to dynamics.',
      );
      efFieldset.appendChild(
        makeNumber('Follower index', ef.index ?? -1, -1, Math.max(-1, efSlots - 1), 1, (value) => {
          stageSlotField(slotState.selected, 'efIndex', value);
          stageSlotEnvelopeField(slotState.selected, 'index', value);
        }, { help: GLOSSARY.ef }),
      );
      const currentFilter = ef.filter_name || (Number.isFinite(Number(ef.filter_index)) ? EF_FILTER_NAMES[Number(ef.filter_index)] : 'LINEAR');
      efFieldset.appendChild(
        makeSelect('Response shape', EF_FILTER_NAMES, currentFilter, (value) => {
          const idx = Math.max(0, EF_FILTER_NAMES.indexOf(value));
          stageSlotEnvelopeField(slotState.selected, 'filter_name', value);
          stageSlotEnvelopeField(slotState.selected, 'filter_index', idx);
        }, { help: GLOSSARY.filter }),
      );
      efFieldset.appendChild(
        makeNumber('Tracking frequency (Hz)', ef.frequency ?? 1000, 0, 20000, 1, (value) => stageSlotEnvelopeField(slotState.selected, 'frequency', value)),
      );
      efFieldset.appendChild(
        makeNumber('Resonance (Q)', ef.q ?? 0.707, 0, 10, 0.01, (value) => stageSlotEnvelopeField(slotState.selected, 'q', value)),
      );
      efFieldset.appendChild(
        makeNumber('Oversample amount', ef.oversample ?? 4, 1, 32, 1, (value) => stageSlotEnvelopeField(slotState.selected, 'oversample', value)),
      );
      efFieldset.appendChild(
        makeNumber('Smoothing', ef.smoothing ?? 0.2, 0, 1, 0.01, (value) => stageSlotEnvelopeField(slotState.selected, 'smoothing', value)),
      );
      efFieldset.appendChild(
        makeNumber('Baseline offset', ef.baseline ?? 0, -10, 10, 0.1, (value) => stageSlotEnvelopeField(slotState.selected, 'baseline', value)),
      );
      efFieldset.appendChild(
        makeNumber('Gain', ef.gain ?? 1, 0, 8, 0.1, (value) => stageSlotEnvelopeField(slotState.selected, 'gain', value)),
      );
      form.appendChild(efFieldset);

      const arg = normalizeArg(slot);
      const argFieldset = makeFieldset(
        'Follower Combiner (ARG)',
        'ARG blends two followers before this slot sends MIDI.',
      );
      argFieldset.appendChild(
        makeToggle('Enable combiner', !!arg.enabled, (value) => stageSlotArgField(slotState.selected, 'enabled', value), { help: GLOSSARY.arg }),
      );
      argFieldset.appendChild(
        makeSelect('Combine method', ARG_METHOD_NAMES, resolveArgMethodName(arg) ?? ARG_METHOD_NAMES[0], (value) => {
          const idx = Math.max(0, ARG_METHOD_NAMES.indexOf(value));
          stageSlotArgField(slotState.selected, 'method', idx);
          stageSlotArgField(slotState.selected, 'method_name', value);
        }),
      );
      argFieldset.appendChild(
        makeNumber('Follower A', arg.sourceA ?? 0, 0, Math.max(0, efSlots - 1), 1, (value) => stageSlotArgField(slotState.selected, 'sourceA', value)),
      );
      argFieldset.appendChild(
        makeNumber('Follower B', arg.sourceB ?? 1, 0, Math.max(0, efSlots - 1), 1, (value) => stageSlotArgField(slotState.selected, 'sourceB', value)),
      );
      form.appendChild(argFieldset);
    }

    formContainer.appendChild(form);
  }

  function makeSelect(labelText, options, current, onChange, { help } = {}) {
    const wrap = document.createElement('label');
    wrap.appendChild(makeControlLabel(labelText, help));
    const select = document.createElement('select');
    options.forEach((opt) => {
      const option = document.createElement('option');
      option.value = opt;
      option.textContent = opt;
      if (opt === current) option.selected = true;
      select.appendChild(option);
    });
    select.onchange = () => onChange(select.value);
    wrap.appendChild(select);
    return wrap;
  }

  function makeNumber(labelText, current, min, max, step, onCommit, { help } = {}) {
    const wrap = document.createElement('label');
    wrap.appendChild(makeControlLabel(labelText, help));
    const input = document.createElement('input');
    input.type = 'number';
    if (min !== undefined && min !== null) input.min = String(min);
    if (max !== undefined && max !== null) input.max = String(max);
    if (step !== undefined && step !== null) input.step = String(step);
    input.value = current;
    input.onchange = () => onCommit(Number(input.value));
    const coarseStep = step ?? 1;
    attachCoarseFine(input, coarseStep);
    wrap.appendChild(input);
    return wrap;
  }

  function makeText(labelText, current, placeholder, onCommit, { help } = {}) {
    const wrap = document.createElement('label');
    wrap.appendChild(makeControlLabel(labelText, help));
    const input = document.createElement('input');
    input.type = 'text';
    input.value = current ?? '';
    if (placeholder) input.placeholder = placeholder;
    input.addEventListener('change', () => onCommit(input.value));
    wrap.appendChild(input);
    return wrap;
  }

  function makeToggle(labelText, current, onCommit, { help } = {}) {
    const wrap = document.createElement('label');
    wrap.className = 'toggle';
    const input = document.createElement('input');
    input.type = 'checkbox';
    input.checked = current;
    input.onchange = () => onCommit(input.checked);
    wrap.append(input, makeControlLabel(labelText, help));
    return wrap;
  }

  function makeControlLabel(text, helpText) {
    const line = document.createElement('span');
    line.className = 'control-label';
    line.textContent = text;
    appendHelpBadge(line, helpText);
    return line;
  }

  function appendHelpBadge(container, helpText) {
    if (!helpText) return;
    const badge = document.createElement('span');
    badge.className = 'help-badge';
    badge.textContent = 'i';
    badge.title = helpText;
    badge.setAttribute('aria-label', helpText);
    badge.setAttribute('role', 'img');
    container.appendChild(badge);
  }

  function makeFieldset(title, hint) {
    const fieldset = document.createElement('fieldset');
    fieldset.className = 'slot-fieldset';
    const legend = document.createElement('legend');
    legend.textContent = title;
    fieldset.appendChild(legend);
    if (hint) {
      const blurb = document.createElement('p');
      blurb.className = 'slot-hint';
      blurb.textContent = hint;
      fieldset.appendChild(blurb);
    }
    return fieldset;
  }

  function attachCoarseFine(input, baseStep) {
    input.addEventListener('keydown', (event) => {
      if (event.key !== 'ArrowUp' && event.key !== 'ArrowDown') return;
      event.preventDefault();
      const delta = (event.key === 'ArrowUp' ? 1 : -1) * (event.shiftKey ? baseStep * 10 : baseStep);
      const next = Math.min(Number(input.max), Math.max(Number(input.min), Number(input.value) + delta));
      input.value = String(next);
      input.dispatchEvent(new Event('change'));
    });
  }

  function stageSlotField(index, key, value) {
    runtime.stage((draft) => {
      draft.slots = draft.slots || [];
      if (!draft.slots[index]) draft.slots[index] = {};
      draft.slots[index][key] = value;
      if (key === 'efIndex') {
        draft.slots[index].ef = draft.slots[index].ef || {};
        draft.slots[index].ef.index = value;
      }
      return draft;
    });
  }

  function stageSlotEnvelopeField(index, key, value) {
    runtime.stage((draft) => {
      draft.slots = draft.slots || [];
      if (!draft.slots[index]) draft.slots[index] = {};
      draft.slots[index].ef = draft.slots[index].ef || {};
      draft.slots[index].ef[key] = value;
      if (key === 'index') {
        draft.slots[index].efIndex = value;
      }
      return draft;
    });
  }

  function stageSlotArgField(index, key, value) {
    runtime.stage((draft) => {
      draft.slots = draft.slots || [];
      if (!draft.slots[index]) draft.slots[index] = {};
      draft.slots[index].arg = draft.slots[index].arg || {};
      draft.slots[index].arg[key] = value;
      return draft;
    });
  }

  function formatNumberField(value, fractionDigits = 2) {
    if (value === null || value === undefined) return '—';
    const num = Number(value);
    if (!Number.isFinite(num)) return '—';
    if (fractionDigits <= 0) return String(Math.round(num));
    return Number(num.toFixed(fractionDigits)).toString();
  }

  function formatEfIndex(index) {
    if (index === null || index === undefined) return '—';
    const num = Number(index);
    if (!Number.isFinite(num)) return '—';
    if (num < 0) return 'Unassigned';
    return `EF ${String(num + 1).padStart(2, '0')}`;
  }

  function formatEfFilter(ef) {
    if (!ef) return '—';
    const index = Number(ef.filter_index);
    const name = ef.filter_name || (Number.isFinite(index) ? EF_FILTER_NAMES[index] : null);
    if (!name) return '—';
    const idx = Number.isFinite(index) ? `#${index}` : null;
    return idx ? `${name} (${idx})` : name;
  }

  function formatEfTuning(ef) {
    if (!ef) return '—';
    const freq = formatNumberField(ef.frequency, 1);
    const q = formatNumberField(ef.q, 2);
    if (freq === '—' && q === '—') return '—';
    const freqLabel = freq === '—' ? '—' : `${freq} Hz`;
    return `${freqLabel} • Q ${q}`;
  }

  function formatEfDynamics(ef) {
    if (!ef) return '—';
    const oversample = Number.isFinite(Number(ef.oversample)) ? Number(ef.oversample) : null;
    const smoothing = formatNumberField(ef.smoothing, 2);
    const oversampleLabel = oversample !== null ? `×${oversample}` : '—';
    if (oversampleLabel === '—' && smoothing === '—') return '—';
    return `Oversample ${oversampleLabel} • Smoothing ${smoothing}`;
  }

  function formatEfBaseline(ef) {
    if (!ef) return '—';
    const baseline = formatNumberField(ef.baseline, 2);
    const gain = formatNumberField(ef.gain, 2);
    if (baseline === '—' && gain === '—') return '—';
    return `Baseline ${baseline} • Gain ${gain}`;
  }

  function resolveArgMethodName(arg) {
    if (!arg || typeof arg !== 'object') return null;
    if (arg.method_name && typeof arg.method_name === 'string') return arg.method_name;
    const index = Number(arg.method);
    if (Number.isFinite(index)) {
      return ARG_METHOD_NAMES[index] || null;
    }
    return null;
  }

  function formatArgMode(arg) {
    if (!arg || typeof arg !== 'object') return '—';
    const methodName = resolveArgMethodName(arg);
    const methodNum = Number(arg.method);
    const methodIndex = Number.isFinite(methodNum) ? `#${methodNum}` : null;
    const enabled = arg.enabled === undefined ? false : Boolean(arg.enabled);
    const pieces = [];
    if (methodName) pieces.push(methodName);
    if (methodIndex && (!methodName || !methodName.includes(methodIndex))) pieces.push(methodIndex);
    pieces.push(enabled ? 'ON' : 'OFF');
    return pieces.join(' · ');
  }

  function formatEfRoute(index) {
    if (index === null || index === undefined) return '—';
    const num = Number(index);
    if (!Number.isFinite(num) || num < 0) return '—';
    return `EF ${String(num + 1).padStart(2, '0')}`;
  }

  function formatArgSources(arg) {
    if (!arg || typeof arg !== 'object') return '—';
    const sourceA = formatEfRoute(arg.sourceA);
    const sourceB = formatEfRoute(arg.sourceB);
    if (sourceA === '—' && sourceB === '—') return '—';
    return `A → ${sourceA} • B → ${sourceB}`;
  }

  function normalizeEf(slot) {
    const base = slot?.ef ? { ...slot.ef } : {};
    const defaults = {
      frequency: 1000,
      q: 0.707,
      oversample: 4,
      smoothing: 0.2,
      baseline: 0,
      gain: 1
    };
    const index = Number.isFinite(slot?.efIndex)
      ? Number(slot.efIndex)
      : Number.isFinite(base.index)
      ? Number(base.index)
      : -1;
    if (!Number.isFinite(base.index)) base.index = index;
    if (Number.isFinite(Number(base.filter_index))) {
      base.filter_index = Number(base.filter_index);
    }
    if (!Number.isFinite(base.filter_index) && typeof base.filter_name === 'string') {
      const idx = EF_FILTER_NAMES.indexOf(base.filter_name);
      if (idx >= 0) base.filter_index = idx;
    }
    if (!base.filter_name && Number.isFinite(base.filter_index)) {
      base.filter_name = EF_FILTER_NAMES[base.filter_index] || null;
    }
    base.frequency = Number.isFinite(Number(base.frequency)) ? Number(base.frequency) : defaults.frequency;
    base.q = Number.isFinite(Number(base.q)) ? Number(base.q) : defaults.q;
    base.oversample = Number.isFinite(Number(base.oversample)) ? Math.max(1, Math.round(Number(base.oversample))) : defaults.oversample;
    const smoothing = Number(base.smoothing);
    base.smoothing = Number.isFinite(smoothing) ? Math.max(0, Math.min(1, smoothing)) : defaults.smoothing;
    base.baseline = Number.isFinite(Number(base.baseline)) ? Number(base.baseline) : defaults.baseline;
    base.gain = Number.isFinite(Number(base.gain)) ? Number(base.gain) : defaults.gain;
    base.index = index;
    return base;
  }

  function normalizeArg(slot) {
    const base = slot?.arg ? { ...slot.arg } : {};
    if (base.enabled === undefined) base.enabled = false;
    const methodIndex = (() => {
      if (base.method_name && typeof base.method_name === 'string') {
        const idx = ARG_METHOD_NAMES.indexOf(base.method_name);
        if (idx >= 0) return idx;
      }
      const idx = Number(base.method);
      return Number.isFinite(idx) ? idx : 0;
    })();
    base.method = methodIndex;
    base.method_name = ARG_METHOD_NAMES[methodIndex] || base.method_name || 'PLUS';
    const manifest = runtime.getState().manifest ?? localManifest;
    const efLimit = Math.max(0, (manifest?.envelope_count ?? localManifest?.envelope_count ?? 6) - 1);
    const sanitizeSource = (value, fallback) => {
      const num = Number(value);
      if (!Number.isFinite(num)) return fallback;
      return Math.max(0, Math.min(efLimit, num));
    };
    base.sourceA = sanitizeSource(base.sourceA, 0);
    base.sourceB = sanitizeSource(base.sourceB, Math.min(1, efLimit));
    return base;
  }

  function normaliseSysexTemplate(value) {
    if (typeof value !== 'string') return '';
    const trimmed = value.trim();
    if (!trimmed.length) return '';
    return trimmed
      .split(/\s+/)
      .map((token) => {
        if (!token.length) return null;
        if (/^(xx|msb|lsb)$/i.test(token)) return token.toUpperCase();
        return token.toUpperCase();
      })
      .filter(Boolean)
      .join(' ');
  }

  function renderLedControls(staged) {
    if (!ledGrid) return;
    ledGrid.innerHTML = '';
    const led = staged?.led ?? { brightness: 0, color: '#000000' };

    const brightnessWrap = document.createElement('label');
    brightnessWrap.className = 'led-control';
    brightnessWrap.textContent = 'Brightness';
    const brightnessInput = document.createElement('input');
    brightnessInput.type = 'range';
    brightnessInput.min = '0';
    brightnessInput.max = '255';
    const initialBrightness = Number.isFinite(Number(led.brightness)) ? Number(led.brightness) : 0;
    brightnessInput.value = String(initialBrightness);
    const brightnessValue = document.createElement('span');
    brightnessValue.className = 'led-value';
    brightnessValue.textContent = String(initialBrightness);
    const pushBrightness = runtime.createThrottle((value) => {
      brightnessValue.textContent = String(value);
      runtime.stage((draft) => {
        draft.led = draft.led || { brightness: 0, color: '#000000' };
        draft.led.brightness = value;
        return draft;
      });
    });
    brightnessInput.addEventListener('input', (event) => {
      const value = Math.min(255, Math.max(0, Math.round(Number(event.target.value))));
      event.target.value = String(value);
      pushBrightness(value);
    });
    brightnessWrap.append(brightnessInput, brightnessValue);

    const colorWrap = document.createElement('label');
    colorWrap.className = 'led-control';
    colorWrap.textContent = 'Color';
    const colorInput = document.createElement('input');
    colorInput.type = 'color';
    const initialColor = typeof led.color === 'string' && /^#([0-9a-fA-F]{6})$/.test(led.color) ? led.color : '#000000';
    colorInput.value = initialColor;
    const colorValue = document.createElement('span');
    colorValue.className = 'led-value';
    colorValue.textContent = initialColor.toUpperCase();
    const pushColor = runtime.createThrottle((value) => {
      const formatted = typeof value === 'string' && /^#([0-9a-fA-F]{6})$/.test(value) ? value.toUpperCase() : '#000000';
      colorValue.textContent = formatted;
      runtime.stage((draft) => {
        draft.led = draft.led || { brightness: 0, color: '#000000' };
        draft.led.color = formatted;
        return draft;
      });
    });
    colorInput.addEventListener('input', (event) => {
      pushColor(event.target.value);
    });
    colorWrap.append(colorInput, colorValue);

    ledGrid.append(brightnessWrap, colorWrap);
  }

  function updateTakeoverGuards(slots) {
    if (!Array.isArray(slots)) return;
    const guardOn = [];
    const guardOff = [];
    slots.forEach((slot, idx) => {
      if (slot?.takeover) guardOff.push(idx);
      else guardOn.push(idx);
    });
    if (guardOn.length) runtime.setPotGuard(guardOn, true);
    if (guardOff.length) runtime.setPotGuard(guardOff, false);
  }

  function renderDeviceMonitor(manifest) {
    if (!deviceMonitor) return;
    deviceMonitor.innerHTML = '';
    const entries = {
      Firmware: manifest?.fw_version || '—',
      'Git SHA': manifest?.git_sha ? manifest.git_sha.slice(0, 8) : '—',
      'Build time': manifest?.build_time || '—',
      'Schema version': manifest?.schema_version ?? '—',
      'Free RAM': Number.isFinite(Number(manifest?.free_ram)) ? `${Math.round(Number(manifest.free_ram) / 1024)} KiB` : '—',
      'Free Flash': Number.isFinite(Number(manifest?.free_flash)) ? `${Math.round(Number(manifest.free_flash) / 1024)} KiB` : '—'
    };
    Object.entries(entries).forEach(([label, value]) => {
      const row = document.createElement('div');
      row.className = 'monitor-row';
      const term = document.createElement('span');
      term.className = 'monitor-label';
      term.textContent = label;
      const val = document.createElement('span');
      val.className = 'monitor-value';
      val.textContent = value;
      row.append(term, val);
      deviceMonitor.appendChild(row);
    });
  }

  function renderSlotButton(el, index, slot) {
    el.className = 'slot-button';
    el.tabIndex = -1;
    el.innerHTML = '';
    el.dataset.index = String(index);
    const label = document.createElement('span');
    label.className = 'slot-label';
    label.textContent = `S${String(index + 1).padStart(2, '0')}`;
    const state = document.createElement('span');
    state.className = 'slot-state';
    state.textContent = slot?.type ?? '—';
    const value = document.createElement('span');
    value.className = 'slot-value';
    value.textContent = slotState.telemetry?.slots?.[index] ?? '0';
    const toggle = document.createElement('button');
    toggle.type = 'button';
    toggle.className = 'takeover';
    toggle.textContent = slot?.takeover ? 'Takeover' : 'Guarded';
    toggle.onclick = () => {
      const next = !slot?.takeover;
      runtime.stage((draft) => {
        draft.slots[index].takeover = next;
        return draft;
      });
      runtime.setPotGuard([index], !next);
    };
    el.append(label, state, value, toggle);
    el.onclick = () => selectSlot(index);
    el.setAttribute('role', 'button');
    el.classList.toggle('selected', index === slotState.selected);
  }

  function renderEfRow(el, index, item) {
    el.className = 'ef-row';
    el.innerHTML = '';
    const label = document.createElement('span');
    label.textContent = `EF ${index + 1}`;
    const input = document.createElement('input');
    input.type = 'number';
    input.min = 0;
    input.max = slotState.slots.length - 1;
    input.value = item?.slot ?? 0;
    input.addEventListener('change', () => {
      runtime.stage((draft) => {
        draft.efSlots = draft.efSlots || [];
        if (!draft.efSlots[index]) draft.efSlots[index] = { slot: 0 };
        draft.efSlots[index].slot = Number(input.value);
        return draft;
      });
    });
    el.append(label, input);
  }

  function paintTelemetry(frame) {
    if (!Array.isArray(frame.slots)) return;
    slotVirtualizer.updateTelemetry(frame.slots);
    frame.envelopes?.forEach((value, idx) => {
      const entry = envMeters[idx];
      if (!entry) return;
      entry.progress.value = value;
      entry.value.textContent = String(value);
    });
    slotDetailValue.textContent = frame.slots[slotState.selected] ?? '—';
  }

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
      value.textContent = '0';
      wrap.append(label, progress, value);
      container.appendChild(wrap);
      meters.push({ wrap, progress, value });
    }
    return meters;
  }

};

if (document.readyState === 'loading') {
  window.addEventListener('DOMContentLoaded', boot, { once: true });
} else {
  boot();
}
