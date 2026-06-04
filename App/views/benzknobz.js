import { createRuntime } from '../runtime.js';
import { FormRenderer } from './form_renderer.js';
import { MidiMonitor } from './midi_monitor.js';
import { presets } from './presets.js';
import { ScopePanel } from './scope_panel.js';
import { normalizeUIMode, readUIModePreference } from './state/ui_preferences.js';
import { createProfileMacroScenePanel } from './panels/profile_macro_scene.js';
import { createSlotEditorPanel } from './panels/slot_editor_panel.js';
import { createDeviceMonitorController } from './controllers/device_monitor_controller.js';
import { createPowerSafetySummary } from './controllers/power_safety_summary.js';
import { createPerformerPanelController } from './controllers/performer_panel_controller.js';
import { createLedControlsController } from './controllers/led_controls_controller.js';
import { createTransportToolbarController } from './controllers/transport_toolbar_controller.js';
import { createUiModeController } from './controllers/ui_mode_controller.js';
import { createDiffStatusController } from './controllers/diff_status_controller.js';
import { createSessionLogController } from './controllers/session_log_controller.js';
import { createPanicHelpController } from './controllers/panic_help_controller.js';
import { createSlotWorkspaceController } from './controllers/slot_workspace_controller.js';
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
  const checkCompatibilityBtn = document.getElementById('check-compatibility');
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
  const transportLaneChip = document.getElementById('transport-lane-chip');
  const connectFailHelp = document.getElementById('connect-fail-help');
  const headerStatus = document.getElementById('header-status');
  const exportPresetBtn = document.getElementById('export-preset');
  const importPresetBtn = document.getElementById('import-preset');
  const presetPicker = document.getElementById('preset-picker');
  const applySaveProfileBtn = document.getElementById('apply-save-profile');
  const simulatorToggle = document.getElementById('simulator-toggle');
  const usbMidiToggleBtn = document.getElementById('usb-midi-toggle');
  const usbMidiStatusEl = document.getElementById('usb-midi-status');
  const noteDynamicsVelocityInput = document.getElementById('note-dynamics-velocity');
  const noteDynamicsProbabilityInput = document.getElementById('note-dynamics-probability');
  const noteDynamicsApplyBtn = document.getElementById('note-dynamics-apply');
  const noteDynamicsStatusEl = document.getElementById('note-dynamics-status');
  const noteDynamicsCard = document.getElementById('note-dynamics-card');
  const noteDynamicsParking = document.getElementById('note-dynamics-parking');
  const deviceClockSourceSelect = document.getElementById('device-clock-source');
  const deviceClockBpmInput = document.getElementById('device-clock-bpm');
  const deviceClockOutSelect = document.getElementById('device-clock-out');
  const deviceClockApplyBtn = document.getElementById('device-clock-apply');
  const deviceClockStatusEl = document.getElementById('device-clock-status');
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
  const powerSafetyPill = document.getElementById('power-safety-pill');
  const performerPanel = document.getElementById('performer-panel');
  const stageConnectBtn = document.getElementById('stage-connect');
  const stageConnectionState = document.getElementById('stage-connection-state');
  const stageDirtyState = document.getElementById('stage-dirty-state');
  const stageDeviceName = document.getElementById('stage-device-name');
  const stageFwVersion = document.getElementById('stage-fw-version');
  const stageProfileSummary = document.getElementById('stage-profile-summary');
  const stageLastEvent = document.getElementById('stage-last-event');
  const stagePowerSummary = document.getElementById('stage-power-summary');
  const stageProfileSelect = document.getElementById('stage-profile-select');
  const stageProfileLoadBtn = document.getElementById('stage-profile-load');
  const stageSceneSelect = document.getElementById('stage-scene-select');
  const stageSceneRecallBtn = document.getElementById('stage-scene-recall');
  const stagePanicHelpBtn = document.getElementById('stage-panic-help');
  const stageSlotGrid = document.getElementById('stage-slots');
  const stageEnvelopeContainer = document.getElementById('stage-envelopes');
  const logEl = document.getElementById('log');
  const sessionLogCount = document.getElementById('session-log-count');
  const sessionLogExportBtn = document.getElementById('session-log-export');
  const sessionLogClearBtn = document.getElementById('session-log-clear');
  const panicHelpDialog = document.getElementById('panic-help-dialog');
  const panicHelpCloseBtn = document.getElementById('panic-help-close');
  const panicHelpContext = document.getElementById('panic-help-context');
  const panicHelpPreflight = document.getElementById('panic-help-preflight');
  const panicHelpVerifyTarget = document.getElementById('panic-help-verify-target');
  const panicHelpExportConfigBtn = document.getElementById('panic-help-export-config');
  const panicHelpExportLogBtn = document.getElementById('panic-help-export-log');
  const panicHelpConfigBootBtn = document.getElementById('panic-help-config-boot');
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
  const arpCard = document.getElementById('arp-profile-card');
  const arpRefreshBtn = document.getElementById('arp-refresh');
  const arpSaveBtn = document.getElementById('arp-save');
  const arpStatusEl = document.getElementById('arp-status');
  const arpLengthInput = document.getElementById('arp-length');
  const arpShapeSelect = document.getElementById('arp-shape');
  const arpSwingInput = document.getElementById('arp-swing');
  const arpGateInput = document.getElementById('arp-gate');
  const arpOctaveInput = document.getElementById('arp-octave');
  const liveArpSlotInput = document.getElementById('live-arp-slot');
  const liveArpLengthInput = document.getElementById('live-arp-length');
  const liveArpShapeSelect = document.getElementById('live-arp-shape');
  const liveArpSwingInput = document.getElementById('live-arp-swing');
  const liveArpGateInput = document.getElementById('live-arp-gate');
  const liveArpOctaveInput = document.getElementById('live-arp-octave');
  const liveArpRefreshBtn = document.getElementById('live-arp-refresh');
  const liveArpApplyBtn = document.getElementById('live-arp-apply');
  const liveArpStartBtn = document.getElementById('live-arp-start');
  const liveArpStopBtn = document.getElementById('live-arp-stop');
  const liveArpStatusEl = document.getElementById('live-arp-status');
  const jitterDepthInput = document.getElementById('jitter-depth');
  const jitterSmoothnessInput = document.getElementById('jitter-smoothness');
  const jitterApplyBtn = document.getElementById('jitter-apply');
  const jitterStatusEl = document.getElementById('jitter-status');
  const lfoCard = document.getElementById('lfo-profile-card');
  const lfoEditor = document.getElementById('lfo-editor');
  const lfoRouteAddBtn = document.getElementById('lfo-route-add');
  const lfoRoutesClearBtn = document.getElementById('lfo-routes-clear');
  const lfoRefreshBtn = document.getElementById('lfo-refresh');
  const lfoSaveBtn = document.getElementById('lfo-save');
  const lfoStatusEl = document.getElementById('lfo-status');
  const modMatrixRefreshBtn = document.getElementById('mod-matrix-refresh');
  const modMatrixBody = document.getElementById('mod-matrix-body');
  const modMatrixStatusEl = document.getElementById('mod-matrix-status');
  const sceneGrid = document.getElementById('scene-grid');
  const sceneStatusEl = document.getElementById('scene-status');
  const uiModeButtons = Array.from(document.querySelectorAll('[data-ui-mode-btn]'));
  const uiModeHint = document.getElementById('ui-mode-hint');
  const advancedTierNodes = Array.from(document.querySelectorAll('[data-ui-tier="advanced"]'));
  const UI_MODE_HINTS = {
    stage: 'Stage mode is a read-only performance dashboard with only show-safe actions.',
    basic: 'Basic mode adds common knob-to-MIDI editing to the Stage dashboard.',
    advanced:
      'Advanced mode adds EF, ARG, filter tuning, and scope diagnostics to the Basic surface.'
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
  const requestedUiMode =
    typeof window !== 'undefined' && typeof window.location === 'object'
      ? new URLSearchParams(window.location.search).get('mode')
      : null;
  const initialUiMode = requestedUiMode ? normalizeUIMode(requestedUiMode) : readUIModePreference();
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

  const diffStatusController = createDiffStatusController({
    runtime,
    onDirtyChanged: updateStagePanel,
    elements: {
      statusEl,
      statusLabel,
      statusMessage,
      diffPanel,
      diffOutput,
      diffEmpty,
      dirtyBadge,
      applyBtn,
      rollbackBtn
    }
  });
  const baseSetStatus = diffStatusController.setStatus;
  const sessionLogController = createSessionLogController({
    logEl,
    countEl: sessionLogCount,
    storage: typeof localStorage === 'undefined' ? null : localStorage,
    exportBtn: sessionLogExportBtn,
    clearBtn: sessionLogClearBtn
  });
  let panicHelpController;
  function exportSessionLogFromPanicHelp() {
    const filename = sessionLogController.exportLog();
    if (!filename) return;
    sessionLogController.recordEvent('SESSION', 'Log exported', filename, 'ok');
    panicHelpController?.render();
  }
  panicHelpController = createPanicHelpController({
    runtime,
    localManifest,
    resolveDeviceName,
    resolveFirmwareVersion,
    getConnectionStage: () => connectionPill?.dataset.stage || 'disconnected',
    getSessionLogCount: () => {
      const countText = sessionLogCount?.textContent || '';
      const parsed = Number.parseInt(countText, 10);
      return Number.isFinite(parsed) ? parsed : 0;
    },
    isSimulatorActive: () => {
      const state = runtime?.getState?.() ?? {};
      return (
        Boolean(simulatorToggle?.classList.contains('active')) ||
        state?.manifest?.fw_version === 'sim-fw'
      );
    },
    getConfigBootDisabled: () => Boolean(configModeBtn?.disabled),
    onExportConfig: () => exportCurrentConfigJson(),
    onExportSessionLog: () => exportSessionLogFromPanicHelp(),
    onRequestConfiguratorBoot: () => configModeBtn?.click(),
    elements: {
      dialog: panicHelpDialog,
      closeBtn: panicHelpCloseBtn,
      contextEl: panicHelpContext,
      preflightEl: panicHelpPreflight,
      verifyTargetEl: panicHelpVerifyTarget,
      exportConfigBtn: panicHelpExportConfigBtn,
      exportLogBtn: panicHelpExportLogBtn,
      configBootBtn: panicHelpConfigBootBtn
    }
  });
  function setStatus(state, label, message) {
    sessionLogController.recordStatus(state, label, message);
    baseSetStatus(state, label, message);
    panicHelpController.render();
  }
  sessionLogController.bind();
  panicHelpController.bind();

  const deviceMonitorController = createDeviceMonitorController({
    container: deviceMonitor,
    resolveDeviceName
  });
  const powerSafetySummary = createPowerSafetySummary({
    containers: [powerSafetyPill, stagePowerSummary]
  });
  const performerPanelController = createPerformerPanelController({
    runtime,
    localManifest,
    resolveDeviceName,
    resolveFirmwareVersion,
    slotTypeAbbreviations: SLOT_TYPE_ABBREVIATIONS,
    connect: () => connectBtn?.click(),
    getConnectionStage: () => connectionPill?.dataset.stage || 'disconnected',
    getConnectionText: () => connectionPill?.textContent || 'Disconnected',
    getProfileText: () => profileSlotStatus?.textContent || 'Slot A - local target',
    getActiveProfileSlot: () => {
      const activeButton = profileSlotButtons.find(
        (button) => button.getAttribute('aria-pressed') === 'true'
      );
      return Number(activeButton?.dataset.profileSlot ?? 0);
    },
    setActiveProfileSlot: (slot) => {
      const selected = Number(slot);
      const button = profileSlotButtons.find(
        (candidate) => Number(candidate.dataset.profileSlot) === selected
      );
      button?.click();
    },
    loadProfile: () => profileLoadBtn?.click(),
    recallScene: (slot) => {
      const selected = Number(slot);
      const recallButton = sceneGrid?.querySelector(
        `[data-scene-slot="${selected}"] .scene-recall`
      );
      recallButton?.click();
    },
    getSelectedSlot: () => slotState.selected,
    selectSlot: (index) => selectSlot(index),
    setStatus,
    openPanicHelp: () => panicHelpController.open(),
    elements: {
      panel: performerPanel,
      connectBtn: stageConnectBtn,
      connectionState: stageConnectionState,
      dirtyState: stageDirtyState,
      deviceName: stageDeviceName,
      fwVersion: stageFwVersion,
      profileSummary: stageProfileSummary,
      lastEvent: stageLastEvent,
      profileSelect: stageProfileSelect,
      profileLoadBtn: stageProfileLoadBtn,
      sceneSelect: stageSceneSelect,
      sceneRecallBtn: stageSceneRecallBtn,
      panicHelpBtn: stagePanicHelpBtn,
      slotGrid: stageSlotGrid,
      envelopeContainer: stageEnvelopeContainer
    }
  });
  const transportToolbarController = createTransportToolbarController({
    runtime,
    runtimeOptions,
    resolveDeviceName,
    resolveFirmwareVersion,
    setStatus,
    syncConfigFileButtons,
    onConnectionPillChanged: updateStagePanel,
    elements: {
      docRoot,
      connectBtn,
      checkCompatibilityBtn,
      configModeBtn,
      applyBtn,
      rollbackBtn,
      simulatorToggle,
      connectionPill,
      connectionBanner,
      transportLaneChip,
      connectFailHelp,
      usbMidiToggleBtn,
      usbMidiStatusEl,
      noteDynamicsVelocityInput,
      noteDynamicsProbabilityInput,
      noteDynamicsApplyBtn,
      noteDynamicsStatusEl,
      deviceClockSourceSelect,
      deviceClockBpmInput,
      deviceClockOutSelect,
      deviceClockApplyBtn,
      deviceClockStatusEl,
      jitterDepthInput,
      jitterSmoothnessInput,
      jitterApplyBtn,
      jitterStatusEl
    }
  });
  const { setConnectionBanner, setConnectionPill, primeCompatibilityStatus } =
    transportToolbarController;

  const migrationCancel = document.getElementById('migration-cancel');
  const migrationExport = document.getElementById('migration-export');

  const slotState = {
    slots: [],
    efSlots: [],
    staged: null,
    selected: 0,
    telemetry: null
  };
  const ledControlsController = createLedControlsController({ container: ledGrid, runtime });
  const uiModeController = createUiModeController({
    docRoot,
    initialMode: initialUiMode,
    hints: UI_MODE_HINTS,
    getSlotCount: () => slotState.slots.length,
    renderSlotEditor,
    setPerformerVisible: (visible) => performerPanelController.setVisible(visible),
    onModeChanged: updateStagePanel,
    elements: {
      uiModeButtons,
      uiModeHint,
      advancedTierNodes,
      editorTabButtons,
      utilityTabButtons,
      utilityPanels,
      efAssignmentCard
    }
  });

  function syncConfigFileButtons() {
    const staged = runtime.getState().staged;
    if (exportPresetBtn) exportPresetBtn.disabled = !staged;
    if (importPresetBtn) importPresetBtn.disabled = false;
  }

  function exportCurrentConfigJson() {
    const { staged } = runtime.getState();
    if (!staged) {
      setStatus(
        'warn',
        'Nothing to export',
        'Connect first or import a configuration before exporting JSON.'
      );
      return;
    }
    const filename = `moarknobz-config-${new Date().toISOString().replace(/[:.]/g, '-')}.json`;
    const blob = new Blob([JSON.stringify(staged, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = filename;
    anchor.click();
    URL.revokeObjectURL(url);
    setStatus('ok', 'Config exported', filename);
  }

  async function importConfigJson(file) {
    if (!file) return;
    const text = await file.text();
    try {
      const json = JSON.parse(text);
      runtime.stage(() => json);
      syncConfigFileButtons();
      setStatus(
        'warn',
        'Config imported',
        'JSON staged locally. Connect and click Apply when you are ready to write it to the device.'
      );
    } catch (err) {
      setStatus('err', 'Import failed', err.message || String(err));
    }
  }

  exportPresetBtn?.addEventListener('click', () => {
    exportCurrentConfigJson();
  });

  importPresetBtn?.addEventListener('click', () => {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json,application/json';
    input.onchange = async () => {
      const file = input.files?.[0];
      await importConfigJson(file);
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
          `${descriptor.label} staged locally. Click Apply to send it to the device.`
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
      applySaveProfileBtn,
      profileHint,
      macroSaveBtn,
      macroRecallBtn,
      macroStatusEl,
      arpCard,
      arpRefreshBtn,
      arpSaveBtn,
      arpStatusEl,
      arpLengthInput,
      arpShapeSelect,
      arpSwingInput,
      arpGateInput,
      arpOctaveInput,
      liveArpSlotInput,
      liveArpLengthInput,
      liveArpShapeSelect,
      liveArpSwingInput,
      liveArpGateInput,
      liveArpOctaveInput,
      liveArpRefreshBtn,
      liveArpApplyBtn,
      liveArpStartBtn,
      liveArpStopBtn,
      liveArpStatusEl,
      lfoCard,
      lfoEditor,
      lfoRouteAddBtn,
      lfoRoutesClearBtn,
      lfoRefreshBtn,
      lfoSaveBtn,
      lfoStatusEl,
      modMatrixRefreshBtn,
      modMatrixBody,
      modMatrixStatusEl,
      sceneGrid,
      sceneStatusEl
    },
    getSelectedSlot: () => slotState.selected
  });
  profileMacroScenePanel.bind();
  const slotEditorPanel = createSlotEditorPanel({
    runtime,
    localManifest,
    slotState,
    formContainer,
    noteDynamicsCard,
    noteDynamicsParking,
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
    setStatus,
    getUiMode: () => uiModeController.getUiMode(),
    getEditorTab: () => uiModeController.getEditorTab()
  });
  const slotWorkspaceController = createSlotWorkspaceController({
    runtime,
    slotState,
    slotContainer,
    efAssignmentGrid,
    envelopeContainer: envContainer,
    slotTypeAbbreviations: SLOT_TYPE_ABBREVIATIONS,
    performerPanel: performerPanelController,
    onSelectSlot: () => populateDetail(),
    onTelemetryPainted: () => populateDetail(),
    onSlotsChanged: () => performerPanelController.renderSlots(slotState.slots)
  });
  const rebuildMeters = (count) => {
    slotWorkspaceController.rebuildMeters(count, 'EF');
    performerPanelController.rebuildMeters(count);
  };
  rebuildMeters(localManifest.envelope_count || 0);

  transportToolbarController.bind();
  performerPanelController.bind();
  slotWorkspaceController.bind();
  uiModeController.bind();
  uiModeController.setUIMode(initialUiMode, { persist: Boolean(requestedUiMode) });
  uiModeController.setEditorTab(uiModeController.getEditorTab());
  uiModeController.setUtilityTab('console');

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
    slotWorkspaceController.paintTelemetry(frame);
    transportToolbarController.onTelemetry(frame);
    deviceMonitorController.renderTelemetry(frame);
  });
  runtime.on('config', ({ staged, config, dirty }) => {
    // `staged` is the single source of truth for editor controls; keep all derived UI panes in
    // sync from this one event to avoid mixed snapshots.
    slotWorkspaceController.syncConfig(staged);
    updateHeader(config);
    diffStatusController.updateDiff(dirty);
    diffStatusController.markDirty(dirty);
    populateDetail();
    ledControlsController.render(staged);
    slotWorkspaceController.updateTakeoverGuards(slotState.slots);
    formRenderer.updateValues();
    syncConfigFileButtons();
    profileMacroScenePanel.onConfigChanged();
    updateStagePanel();
    panicHelpController.render();
  });
  runtime.on('manifest', (manifest) => {
    updateHeaderManifest(manifest);
    deviceMonitorController.renderManifest(manifest);
    updatePowerSafetySummary(manifest);
    profileMacroScenePanel.onManifest(manifest);
    transportToolbarController.onManifest(manifest);
    const followerCount = Number.isFinite(Number(manifest?.envelope_count))
      ? Number(manifest.envelope_count)
      : localManifest.envelope_count || 0;
    rebuildMeters(followerCount);
    updateStagePanel();
    panicHelpController.render();
  });
  runtime.on('log', (line) => {
    sessionLogController.recordEvent('RUNTIME', 'Raw line', line, 'info');
    panicHelpController.render();
  });
  runtime.on('validation-error', (errors) => {
    diffStatusController.showValidationErrors(errors);
    sessionLogController.recordEvent(
      'VALIDATION',
      'Schema validation failed',
      `${Array.isArray(errors) ? errors.length : 0} error(s)`,
      'err'
    );
    panicHelpController.render();
  });
  runtime.on('applied', ({ checksum }) => {
    diffStatusController.clearApplied(checksum);
    sessionLogController.recordEvent(
      'APPLY',
      'Device acknowledged staged edits',
      `Checksum ${String(checksum).slice(0, 8)}...`,
      'ok'
    );
    panicHelpController.render();
  });
  runtime.on('migration-required', ({ from, to, canAdapt }) => {
    sessionLogController.recordEvent(
      'MIGRATION',
      'Schema migration required',
      `${from} -> ${to}${canAdapt ? ' (adapter available)' : ''}`,
      'warn'
    );
    panicHelpController.render();
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
    syncConfigFileButtons();
    setStatus('ok', 'Connected', 'Schema synced. Stage edits before applying.');
    sessionLogController.recordEvent(
      'CONNECTION',
      'Connected',
      `${resolveDeviceName(manifest)} • fw ${resolveFirmwareVersion(manifest)} • schema ${
        manifest?.schema_version ?? '?'
      }`,
      'ok'
    );
    profileMacroScenePanel.onConnected();
    transportToolbarController.onConnected();
    updateStagePanel();
    panicHelpController.render();
  });
  runtime.on('disconnected', () => {
    // Mirror the connected handler in reverse so stale controls cannot issue RPCs offline.
    setConnectionPill('disconnected', 'Disconnected');
    setConnectionBanner('disconnected', runtime.getState().manifest);
    if (applyBtn) applyBtn.disabled = true;
    if (rollbackBtn) rollbackBtn.disabled = true;
    syncConfigFileButtons();
    setStatus('warn', 'Disconnected', 'Reconnect to continue editing.');
    sessionLogController.recordEvent('CONNECTION', 'Disconnected', '', 'warn');
    profileMacroScenePanel.onDisconnected();
    transportToolbarController.onDisconnected();
    updateStagePanel();
    panicHelpController.render();
  });
  runtime.on('error', (err) => {
    // Runtime errors are treated as hard disconnects from the UI perspective.
    setConnectionPill('disconnected', 'Disconnected');
    setConnectionBanner('disconnected', runtime.getState().manifest);
    connectFailHelp?.setAttribute('open', '');
    setStatus('err', 'Runtime error', err.message || String(err));
    sessionLogController.recordEvent('RUNTIME', 'Error', err.message || String(err), 'err');
    profileMacroScenePanel.onRuntimeError();
    transportToolbarController.onDisconnected();
    updateStagePanel();
    panicHelpController.render();
  });
  runtime.on('macro', (payload) => profileMacroScenePanel.onMacro(payload));
  runtime.on('scene', (payload) => {
    profileMacroScenePanel.onScene(payload);
    updateStagePanel();
  });
  runtime.on('rollback', () => {
    diffStatusController.updateDiff(false);
    diffStatusController.markDirty(false);
    setStatus('warn', 'Rollback', 'Local edits were discarded.');
    panicHelpController.render();
  });

  runtime.restoreLocalState();
  updatePowerSafetySummary(runtime.getState().manifest ?? localManifest);
  updateStagePanel();
  syncConfigFileButtons();
  panicHelpController.render();
  primeCompatibilityStatus();
  new MidiMonitor({ container: document.getElementById('midi-panel') });
  new ScopePanel({
    container: document.getElementById('scope-panel'),
    runtime,
    manifest: localManifest
  });

  function updatePowerSafetySummary(manifest) {
    powerSafetySummary.render(manifest);
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
    updatePowerSafetySummary(manifest);
    updateHeader(runtime.getState().live);
    updateStagePanel();
  }

  function updateStagePanel() {
    const selected = Number(stageSceneSelect?.value ?? 0);
    const recallButton = sceneGrid?.querySelector(`[data-scene-slot="${selected}"] .scene-recall`);
    performerPanelController.refresh({
      connected: connectionPill?.dataset.stage === 'live',
      profileLoadDisabled: Boolean(profileLoadBtn?.disabled),
      sceneRecallDisabled: !recallButton || Boolean(recallButton.disabled)
    });
  }

  // Change which slot is focused in the inspector/editor pane.
  function selectSlot(index) {
    slotWorkspaceController.selectSlot(index);
  }

  // Fill the slot detail card from the selected slot plus latest telemetry.
  function populateDetail() {
    profileMacroScenePanel.setLiveArpSlot(slotState.selected);
    slotEditorPanel.populateDetail();
  }

  // Rebuild the right-hand slot editor for the current selection and UI tier.
  function renderSlotEditor() {
    slotEditorPanel.renderSlotEditor();
  }
};

if (document.readyState === 'loading') {
  window.addEventListener('DOMContentLoaded', boot, { once: true });
} else {
  boot();
}
