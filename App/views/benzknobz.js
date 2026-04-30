import { createRuntime } from '../runtime.js';
import { FormRenderer } from './form_renderer.js';
import { MidiMonitor } from './midi_monitor.js';
import { presets } from './presets.js';
import { ScopePanel } from './scope_panel.js';
import { VirtualGrid, VirtualList } from './virtualizers.js';
import { normalizeUIMode, persistUIMode, readUIModePreference } from './state/ui_preferences.js';
import { createProfileMacroScenePanel } from './panels/profile_macro_scene.js';
import { createSlotEditorPanel } from './panels/slot_editor_panel.js';
import {
  EF_FILTER_NAMES,
  SLOT_TYPE_NAMES,
  ARG_METHOD_NAMES,
  formatArgMethodLabel,
  describeArgMethod
} from '../lib/constants.js';
import { createLocalManifest } from '../manifest_contract.js';

const localManifest = createLocalManifest({
  uiVersion: '2026.03.09',
  argMethodCount: ARG_METHOD_NAMES.length
});

const SLOT_ROW_HEIGHT = 82;
const EF_ROW_HEIGHT = 44;

const SLOT_TYPE_ABBREVIATIONS = {
  OFF: 'OFF',
  CC: 'CC',
  Note: 'NOTE',
  PitchBend: 'PB',
  ProgramChange: 'PC',
  Aftertouch: 'AT',
  ModWheel: 'MW',
  NRPN: 'NRPN',
  RPN: 'RPN',
  SysEx: 'SX'
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

// Page bootstrap owns the full operator shell: transport controls, staged/live
// config rendering, recovery affordances, and telemetry.
const boot = () => {
  if (typeof document === 'undefined') return;
  const docRoot = document.documentElement;
  if (docRoot?.dataset?.mn42Booted === 'true') return;
  if (docRoot) docRoot.dataset.mn42Booted = 'true';
  const statusEl = document.getElementById('status');
  const statusLabel = document.getElementById('status-label');
  const statusMessage = statusEl?.querySelector('.status-message');
  const connectBtn = document.getElementById('connect');
  const configModeBtn = document.getElementById('config-mode');
  const applyBtn = document.getElementById('apply');
  const rollbackBtn = document.getElementById('rollback');
  const slotContainer = document.getElementById('slots');
  const envContainer = document.getElementById('envelopes');
  const diffPanel = document.getElementById('diff-panel');
  const diffOutput = document.getElementById('diff-output');
  const dirtyBadge = document.getElementById('dirty-badge');
  const connectionPill = document.getElementById('connection-pill');
  const connectionBanner = document.getElementById('connection-banner');
  const connectFailHelp = document.getElementById('connect-fail-help');
  const headerStatus = document.getElementById('header-status');
  const exportPresetBtn = document.getElementById('export-preset');
  const importPresetBtn = document.getElementById('import-preset');
  const presetPicker = document.getElementById('preset-picker');
  const simulatorToggle = document.getElementById('simulator-toggle');
  const ledGrid = document.getElementById('led-grid');
  const formContainer = document.getElementById('form');
  const editorTabButtons = Array.from(document.querySelectorAll('[data-editor-tab]'));
  const utilityTabButtons = Array.from(document.querySelectorAll('[data-utility-tab]'));
  const utilityPanels = Array.from(document.querySelectorAll('[data-utility-panel]'));
  const diffEmpty = document.getElementById('diff-empty');
  const schemaSections = Array.from(document.querySelectorAll('[data-schema-target]')).map(
    (element) => ({
      target: element,
      schemaPath: element.dataset.schemaTarget
    })
  );
  const formRenderer = new FormRenderer({ runtime, sections: schemaSections });
  const efAssignmentCard = document.getElementById('ef-assignment-card');
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
  const profileHint = document.getElementById('profile-hint');
  const macroSaveBtn = document.getElementById('macro-save');
  const macroRecallBtn = document.getElementById('macro-recall');
  const macroStatusEl = document.getElementById('macro-status');
  const sceneGrid = document.getElementById('scene-grid');
  const sceneStatusEl = document.getElementById('scene-status');
  const uiModeButtons = Array.from(document.querySelectorAll('[data-ui-mode-btn]'));
  const uiModeHint = document.getElementById('ui-mode-hint');
  const advancedTierNodes = Array.from(document.querySelectorAll('[data-ui-tier="advanced"]'));
  const UI_MODE_HINTS = {
    basic: 'Basic mode keeps common knob-to-MIDI mapping controls visible.',
    advanced: 'Advanced mode reveals EF, ARG, filter tuning, and scope diagnostics.'
  };
  const GLOSSARY = {
    mapping:
      'Knob to MIDI mapping: choose the message type, channel, and number your synth or DAW expects.',
    takeover: 'Take Control waits for the knob to pass the current value so tweaks do not jump.',
    browserLocal:
      'Stored in this browser only. It is not sent to firmware and will not come back from the device on reconnect.',
    ef: 'EF (Envelope Follower) tracks input level to drive dynamic modulation.',
    arg: 'ARG combines two envelope followers with a math method before mapping to MIDI.',
    filter: 'Filter shape and tuning control how aggressively the envelope follower reacts.',
    sysex:
      'SysEx template uses hex bytes; XX, MSB, and LSB placeholders are replaced with live values.'
  };
  const migrationDialog = document.getElementById('migration-dialog');
  let activeUiMode = readUIModePreference();
  let activeEditorTab = 'mapping';
  let activeUtilityTab = 'console';
  const migrationPreview = document.getElementById('migration-preview');
  const migrationApply = document.getElementById('migration-apply');

  // Derive the device label shown in the header/banner from the latest manifest.
  function resolveDeviceName(manifest) {
    const candidate = manifest?.device_name ?? manifest?.product_name ?? localManifest.device_name;
    if (typeof candidate !== 'string') return localManifest.device_name;
    const trimmed = candidate.trim();
    return trimmed || localManifest.device_name;
  }

  // Normalize the reported firmware version so the UI always shows something.
  function resolveFirmwareVersion(manifest) {
    const candidate = manifest?.fw_version;
    if (typeof candidate !== 'string') return 'unknown';
    const trimmed = candidate.trim();
    return trimmed || 'unknown';
  }

  // Update the banner line that identifies the connected deck.
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

  // Update the compact connection status pill in the header.
  function setConnectionPill(stage, text) {
    if (!connectionPill) return;
    connectionPill.dataset.stage = stage;
    connectionPill.textContent = text;
  }
  const migrationCancel = document.getElementById('migration-cancel');
  const migrationExport = document.getElementById('migration-export');

  const slotState = {
    slots: [],
    efSlots: [],
    staged: null,
    selected: 0,
    telemetry: null
  };
  const ledControlState = {
    mounted: false,
    brightnessInput: null,
    brightnessValue: null,
    colorInput: null,
    colorValue: null
  };

  const slotVirtualizer = new VirtualGrid(slotContainer, {
    columns: 6,
    rowHeight: SLOT_ROW_HEIGHT,
    render: renderSlotButton
  });

  const efVirtualizer = new VirtualList(efAssignmentGrid, {
    itemHeight: EF_ROW_HEIGHT,
    render: renderEfRow
  });

  let envMeters = [];
  // Rebuild the envelope meter widgets whenever the manifest changes follower count.
  const rebuildMeters = (count) => {
    envMeters = initializeMeters(envContainer, count, 'EF');
  };
  rebuildMeters(localManifest.envelope_count || 0);
  slotVirtualizer.setData([]);

  connectBtn?.addEventListener('click', async () => {
    try {
      setConnectionPill('handshake', 'Handshaking…');
      setConnectionBanner('handshake', runtime.getState().manifest);
      await runtime.connect();
    } catch (err) {
      setConnectionPill('disconnected', 'Disconnected');
      setConnectionBanner('disconnected', runtime.getState().manifest);
      connectFailHelp?.setAttribute('open', '');
      setStatus('err', 'Connect failed', err.message || String(err));
    }
  });

  configModeBtn?.addEventListener('click', async () => {
    try {
      if (configModeBtn) configModeBtn.disabled = true;
      setConnectionPill('handshake', 'Config boot…');
      setConnectionBanner('handshake', runtime.getState().manifest);
      await runtime.requestConfiguratorBoot();
      setConnectionPill('disconnected', 'Rebooting');
      setConnectionBanner('disconnected', runtime.getState().manifest);
      setStatus('ok', 'Config boot', 'Reconnect after the USB device reappears.');
    } catch (err) {
      setConnectionPill('disconnected', 'Disconnected');
      setConnectionBanner('disconnected', runtime.getState().manifest);
      connectFailHelp?.setAttribute('open', '');
      setStatus('err', 'Config boot failed', err.message || String(err));
    } finally {
      if (configModeBtn) configModeBtn.disabled = false;
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
    simulatorToggle.setAttribute(
      'aria-pressed',
      simulatorToggle.classList.contains('active') ? 'true' : 'false'
    );
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
  editorTabButtons.forEach((button) => {
    button.addEventListener('click', () => {
      setEditorTab(button.dataset.editorTab);
    });
  });
  utilityTabButtons.forEach((button) => {
    button.addEventListener('click', () => {
      setUtilityTab(button.dataset.utilityTab);
    });
  });
  setUIMode(activeUiMode, { persist: false });
  setEditorTab(activeEditorTab);
  setUtilityTab(activeUtilityTab);

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
        setStatus(
          'warn',
          'Preset staged',
          `${descriptor.label} staged. Hit Apply to push it to the deck.`
        );
      } catch (err) {
        setStatus('err', 'Preset load failed', err.message || String(err));
      } finally {
        presetPicker.selectedIndex = 0;
      }
    });
  }

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

  const profileMacroScenePanel = createProfileMacroScenePanel({
    runtime,
    formRenderer,
    localManifest,
    setStatus,
    elements: {
      profileSlotButtons,
      profileSlotStatus,
      profileSaveBtn,
      profileLoadBtn,
      profileResetBtn,
      profileWizardTarget,
      profileWizardSwitchBtn,
      profileWizardApplyBtn,
      profileWizardSaveBtn,
      profileWizardStatus,
      profileDownloadBtn,
      profileUploadBtn,
      profileHint,
      macroSaveBtn,
      macroRecallBtn,
      macroStatusEl,
      sceneGrid,
      sceneStatusEl
    }
  });
  profileMacroScenePanel.bind();
  const slotEditorPanel = createSlotEditorPanel({
    runtime,
    localManifest,
    slotState,
    formContainer,
    detailElements: {
      slotDetailIndex,
      slotDetailStatus,
      slotDetailType,
      slotDetailChannel,
      slotDetailData,
      slotDetailEfIndex,
      slotDetailEfFilter,
      slotDetailEfTuning,
      slotDetailEfDynamics,
      slotDetailEfBaseline,
      slotDetailArg,
      slotDetailArgSources,
      slotDetailValue
    },
    glossary: GLOSSARY,
    slotTypeNames: SLOT_TYPE_NAMES,
    efFilterNames: EF_FILTER_NAMES,
    argMethodNames: ARG_METHOD_NAMES,
    formatArgMethodLabel,
    describeArgMethod,
    getUiMode: () => activeUiMode,
    getEditorTab: () => activeEditorTab
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
    profileMacroScenePanel.onConfigChanged();
  });
  runtime.on('manifest', (manifest) => {
    updateHeaderManifest(manifest);
    renderDeviceMonitor(manifest);
    profileMacroScenePanel.onManifest(manifest);
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
    diffOutput.textContent = `Schema violations:\n${errors
      .map((e) => `• ${e.instancePath || '/'} ${e.message}`)
      .join('\n')}`;
  });
  runtime.on('applied', ({ checksum }) => {
    diffPanel?.setAttribute('hidden', '');
    diffOutput.textContent = '';
    setStatus('ok', 'Device synced', `Checksum ${checksum.slice(0, 8)}…`);
  });
  runtime.on('migration-required', ({ from, to, canAdapt }) => {
    if (!migrationDialog || !migrationPreview) return;
    migrationPreview.textContent = `Firmware schema ${from} vs UI ${to}. ${
      canAdapt
        ? 'An adapter is available; stage edits then apply.'
        : 'Export your preset and update firmware/UI to continue.'
    }`;
    migrationDialog.showModal();
  });
  runtime.on('connected', ({ manifest }) => {
    // Connection flips the entire toolbar/profile surface into interactive mode.
    setConnectionPill('live', 'Connected');
    setConnectionBanner('live', manifest);
    connectFailHelp?.removeAttribute('open');
    if (applyBtn) applyBtn.disabled = true;
    if (rollbackBtn) rollbackBtn.disabled = true;
    if (exportPresetBtn) exportPresetBtn.disabled = false;
    if (importPresetBtn) importPresetBtn.disabled = false;
    setStatus('ok', 'Connected', 'Schema synced. Stage edits before applying.');
    profileMacroScenePanel.onConnected();
  });
  runtime.on('disconnected', () => {
    // Mirror the connected handler in reverse so stale controls cannot issue RPCs offline.
    setConnectionPill('disconnected', 'Disconnected');
    setConnectionBanner('disconnected', runtime.getState().manifest);
    if (applyBtn) applyBtn.disabled = true;
    if (rollbackBtn) rollbackBtn.disabled = true;
    if (exportPresetBtn) exportPresetBtn.disabled = true;
    if (importPresetBtn) importPresetBtn.disabled = true;
    setStatus('warn', 'Disconnected', 'Reconnect to continue editing.');
    profileMacroScenePanel.onDisconnected();
  });
  runtime.on('error', (err) => {
    // Runtime errors are treated as hard disconnects from the UI perspective.
    setConnectionPill('disconnected', 'Disconnected');
    setConnectionBanner('disconnected', runtime.getState().manifest);
    connectFailHelp?.setAttribute('open', '');
    setStatus('err', 'Runtime error', err.message || String(err));
    profileMacroScenePanel.onRuntimeError();
  });
  runtime.on('macro', (payload) => profileMacroScenePanel.onMacro(payload));
  runtime.on('scene', (payload) => profileMacroScenePanel.onScene(payload));
  runtime.on('rollback', () => {
    updateDiff(false);
    markDirty(false);
    setStatus('warn', 'Rollback', 'Local edits were discarded.');
  });

  runtime.restoreLocalState();
  new MidiMonitor({ container: document.getElementById('midi-panel') });
  new ScopePanel({
    container: document.getElementById('scope-panel'),
    runtime,
    manifest: localManifest
  });

  function setStatus(state, label, message) {
    // Single status sink used by transport, schema, profile, macro, and scene flows.
    if (!statusEl || !statusLabel || !statusMessage) return;
    statusEl.dataset.state = state;
    statusLabel.textContent = label;
    statusMessage.textContent = message;
  }

  // Limit the slot editor to the supported tabs.
  function normalizeEditorTab(tab) {
    return tab === 'envelope' || tab === 'arg' ? tab : 'mapping';
  }

  // Limit the utility rail to the supported tabs.
  function normalizeUtilityTab(tab) {
    return tab === 'diff' || tab === 'midi' || tab === 'scope' ? tab : 'console';
  }

  // Flip between Basic and Advanced presentation without altering the staged config.
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
    if (activeUiMode !== 'advanced' && activeEditorTab !== 'mapping') {
      activeEditorTab = 'mapping';
    }
    if (activeUiMode !== 'advanced' && activeUtilityTab === 'scope') {
      activeUtilityTab = 'console';
    }
    refreshEditorTabs();
    refreshUtilityTabs();
    if (slotState.slots.length) renderSlotEditor();
  }

  // Update the selected-slot editor tab chrome and tab-tied helper panels.
  function refreshEditorTabs() {
    const effectiveTab =
      activeUiMode === 'advanced' ? normalizeEditorTab(activeEditorTab) : 'mapping';
    activeEditorTab = effectiveTab;
    editorTabButtons.forEach((button) => {
      const buttonTab = normalizeEditorTab(button.dataset.editorTab);
      button.setAttribute('aria-pressed', buttonTab === effectiveTab ? 'true' : 'false');
    });
    if (efAssignmentCard) {
      efAssignmentCard.toggleAttribute(
        'hidden',
        !(activeUiMode === 'advanced' && effectiveTab === 'envelope')
      );
    }
  }

  // Update the right-rail utility tab chrome and show only the active panel.
  function refreshUtilityTabs() {
    const effectiveTab =
      activeUiMode === 'advanced'
        ? normalizeUtilityTab(activeUtilityTab)
        : normalizeUtilityTab(activeUtilityTab === 'scope' ? 'console' : activeUtilityTab);
    activeUtilityTab = effectiveTab;
    utilityTabButtons.forEach((button) => {
      const buttonTab = normalizeUtilityTab(button.dataset.utilityTab);
      button.setAttribute('aria-pressed', buttonTab === effectiveTab ? 'true' : 'false');
    });
    utilityPanels.forEach((panel) => {
      const panelTab = normalizeUtilityTab(panel.dataset.utilityPanel);
      panel.classList.toggle('utility-panel-active', panelTab === effectiveTab);
      panel.toggleAttribute('hidden', panelTab !== effectiveTab);
    });
  }

  // Change the selected-slot editor tab and re-render the editor body.
  function setEditorTab(tab) {
    activeEditorTab = normalizeEditorTab(tab);
    refreshEditorTabs();
    if (slotState.slots.length) {
      renderSlotEditor();
    }
  }

  // Change the active utility panel in the right rail.
  function setUtilityTab(tab) {
    activeUtilityTab = normalizeUtilityTab(tab);
    refreshUtilityTabs();
  }

  // Toggle the dirty badge and the Apply/Rollback affordances together.
  function markDirty(isDirty) {
    console.debug('[UI] markDirty', isDirty);
    if (dirtyBadge) dirtyBadge.toggleAttribute('hidden', !isDirty);
    if (applyBtn) applyBtn.disabled = !isDirty;
    if (rollbackBtn) rollbackBtn.disabled = !isDirty;
  }

  // Render a readable summary of staged changes compared with the live config.
  function updateDiff(isDirty) {
    if (!diffPanel || !diffOutput) return;
    const changes = runtime.diff();
    if (!isDirty || !changes.length) {
      diffPanel.setAttribute('hidden', '');
      diffOutput.textContent = '';
      diffEmpty?.removeAttribute('hidden');
      return;
    }
    diffPanel.removeAttribute('hidden');
    diffEmpty?.setAttribute('hidden', '');
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

  // Condense arbitrary values so the diff panel stays scannable.
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

  // Best-effort JSON formatting for diff previews.
  function stringifyDiffValue(value) {
    try {
      return JSON.stringify(value);
    } catch {
      return '[unserializable]';
    }
  }

  // Trim long diff strings without losing the fact that they were truncated.
  function truncateDiffText(text, maxLength) {
    if (typeof text !== 'string') return '';
    if (text.length <= maxLength) return text;
    return `${text.slice(0, Math.max(0, maxLength - 1))}…`;
  }

  // Refresh the compact firmware/schema/memory summary in the page header.
  function updateHeader(config) {
    if (!headerStatus) return;
    const manifest = runtime.getState().manifest;
    const ramBytes = Number(manifest?.free_ram);
    const flashBytes = Number(manifest?.free_flash);
    const ram = Number.isFinite(ramBytes) ? `${Math.round(ramBytes / 1024)}k RAM` : 'ram?';
    const flash = Number.isFinite(flashBytes)
      ? `${Math.round(flashBytes / 1024)}k flash`
      : 'flash?';
    headerStatus.textContent = [
      manifest?.fw_version || 'fw?',
      manifest?.schema_version ?? 'schema?',
      `${ram} • ${flash}`
    ].join(' • ');
  }

  // Promote the latest manifest into the header and connection chrome.
  function updateHeaderManifest(manifest) {
    setConnectionPill('live', 'Connected');
    setConnectionBanner('live', manifest);
    updateHeader(runtime.getState().live);
  }

  // Change which slot is focused in the inspector/editor pane.
  function selectSlot(index) {
    const maxIndex = Math.max(0, slotState.slots.length - 1);
    slotState.selected = Math.min(Math.max(0, index), maxIndex);
    slotVirtualizer.highlight(slotState.selected);
    slotVirtualizer.scrollToIndex(slotState.selected);
    populateDetail();
  }

  // Fill the slot detail card from the selected slot plus latest telemetry.
  function populateDetail() {
    slotEditorPanel.populateDetail();
  }

  // Rebuild the right-hand slot editor for the current selection and UI tier.
  function renderSlotEditor() {
    slotEditorPanel.renderSlotEditor();
  }

  // Render the small LED section and throttle slider/color updates into staged state.
  function renderLedControls(staged) {
    if (!ledGrid) return;
    const led = staged?.led ?? { brightness: 0, color: '#000000' };
    const initialBrightness = Number.isFinite(Number(led.brightness)) ? Number(led.brightness) : 0;
    const initialColor =
      typeof led.color === 'string' && /^#([0-9a-fA-F]{6})$/.test(led.color)
        ? led.color
        : '#000000';

    if (!ledControlState.mounted) {
      ledGrid.innerHTML = '';

      const brightnessWrap = document.createElement('label');
      brightnessWrap.className = 'led-control';
      brightnessWrap.textContent = 'Brightness';
      const brightnessInput = document.createElement('input');
      brightnessInput.type = 'range';
      brightnessInput.min = '0';
      brightnessInput.max = '255';
      const brightnessValue = document.createElement('span');
      brightnessValue.className = 'led-value';
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
      const colorValue = document.createElement('span');
      colorValue.className = 'led-value';
      const pushColor = runtime.createThrottle((value) => {
        const formatted =
          typeof value === 'string' && /^#([0-9a-fA-F]{6})$/.test(value)
            ? value.toUpperCase()
            : '#000000';
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
      ledControlState.mounted = true;
      ledControlState.brightnessInput = brightnessInput;
      ledControlState.brightnessValue = brightnessValue;
      ledControlState.colorInput = colorInput;
      ledControlState.colorValue = colorValue;
    }

    if (ledControlState.brightnessInput) {
      ledControlState.brightnessInput.value = String(initialBrightness);
    }
    if (ledControlState.brightnessValue) {
      ledControlState.brightnessValue.textContent = String(initialBrightness);
    }
    if (ledControlState.colorValue) {
      ledControlState.colorValue.textContent = initialColor.toUpperCase();
    }
    if (ledControlState.colorInput && document.activeElement !== ledControlState.colorInput) {
      ledControlState.colorInput.value = initialColor;
    }
  }

  // Reapply browser-only pickup guards after staged/live slot data changes.
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

  // Paint the diagnostics card from the latest manifest/build information.
  function renderDeviceMonitor(manifest) {
    if (!deviceMonitor) return;
    deviceMonitor.innerHTML = '';
    const entries = {
      Device: resolveDeviceName(manifest),
      Firmware: manifest?.fw_version || '—',
      'Git SHA': manifest?.git_sha ? manifest.git_sha.slice(0, 8) : '—',
      'Build time': manifest?.build_time || '—',
      'Schema version': manifest?.schema_version ?? '—',
      'Free RAM': Number.isFinite(Number(manifest?.free_ram))
        ? `${Math.round(Number(manifest.free_ram) / 1024)} KiB`
        : '—',
      'Free Flash': Number.isFinite(Number(manifest?.free_flash))
        ? `${Math.round(Number(manifest.free_flash) / 1024)} KiB`
        : '—'
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

  // Render one slot tile inside the virtualized grid.
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
    state.textContent = SLOT_TYPE_ABBREVIATIONS[slot?.type] ?? slot?.type ?? '—';
    state.title = slot?.type ?? 'Unassigned';
    const toggle = document.createElement('button');
    toggle.type = 'button';
    toggle.className = 'takeover';
    toggle.textContent = slot?.takeover ? 'PK' : 'IM';
    toggle.title = slot?.takeover
      ? 'Browser-only pickup guard enabled'
      : 'Immediate local response';
    toggle.setAttribute('aria-label', slot?.takeover ? 'Pickup mode' : 'Immediate mode');
    toggle.onclick = () => {
      const next = !slot?.takeover;
      runtime.setLocalSlotMeta(index, { takeover: next });
      runtime.setPotGuard([index], !next);
    };
    el.append(label, state, toggle);
    el.onclick = () => selectSlot(index);
    el.setAttribute('role', 'button');
    el.classList.toggle('selected', index === slotState.selected);
  }

  // Normalize the follower-assignment list format into a bounded unique slot array.
  function normalizeFollowerSlotList(item, maxSlotIndex) {
    const source = Array.isArray(item?.slots)
      ? item.slots
      : item?.slot !== undefined && item?.slot !== null
        ? [item.slot]
        : [];
    const seen = new Set();
    const normalized = [];
    source.forEach((candidate) => {
      const numeric = Number(candidate);
      if (!Number.isFinite(numeric)) return;
      const rounded = Math.round(numeric);
      if (rounded < 0) return;
      const bounded = Math.max(0, Math.min(maxSlotIndex, rounded));
      if (seen.has(bounded)) return;
      seen.add(bounded);
      normalized.push(bounded);
    });
    normalized.sort((a, b) => a - b);
    return normalized;
  }

  // Parse the freeform follower-assignment text input into normalized slot ids.
  function parseFollowerSlotsInput(text, maxSlotIndex) {
    if (typeof text !== 'string' || !text.trim()) return [];
    const tokens = text
      .split(/[,\s]+/)
      .map((token) => token.trim())
      .filter(Boolean);
    return normalizeFollowerSlotList({ slots: tokens }, maxSlotIndex);
  }

  // Convert a follower slot list back into editable text.
  function formatFollowerSlots(slots) {
    if (!Array.isArray(slots) || !slots.length) return '';
    return slots.join(', ');
  }

  // Render one row in the EF assignment editor.
  function renderEfRow(el, index, item) {
    el.className = 'ef-row';
    el.innerHTML = '';
    const maxSlotIndex = Math.max(0, slotState.slots.length - 1);
    const normalizedSlots = normalizeFollowerSlotList(item, maxSlotIndex);
    const label = document.createElement('span');
    label.className = 'ef-row-label';
    label.textContent = `EF ${index + 1}`;
    const input = document.createElement('input');
    input.type = 'text';
    input.placeholder = '3, 10, 17';
    input.value = formatFollowerSlots(normalizedSlots);
    input.addEventListener('change', () => {
      const parsed = parseFollowerSlotsInput(input.value, maxSlotIndex);
      input.value = formatFollowerSlots(parsed);
      runtime.stage((draft) => {
        draft.efSlots = draft.efSlots || [];
        if (!draft.efSlots[index] || typeof draft.efSlots[index] !== 'object') {
          draft.efSlots[index] = { slots: [] };
        }
        draft.efSlots[index].slots = parsed;
        delete draft.efSlots[index].slot;
        return draft;
      });
    });
    el.append(label, input);
  }

  // Push the latest telemetry frame into the slot grid, envelope meters, and detail view.
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

  // Build a simple progress-meter bank for envelope telemetry.
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
