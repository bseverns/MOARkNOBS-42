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
import { describeDeviceConfigPatch } from '../lib/tuning_catalog.js';
import { createLocalManifest } from '../manifest_contract.js';

// BenzKnobz skin over the shared runtime. Runtime owns the protocol; this file
// owns the stage props: slots, profiles, LEDs, logs, and the workshop simulator.
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
// config rendering, recovery affordances, and telemetry. Start here, then jump
// to runtime.js when a button crosses the serial line.
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
  const retryReadbackBtn = document.getElementById('retry-readback');
  const slotContainer = document.getElementById('slots');
  const envContainer = document.getElementById('envelopes');
  const diffPanel = document.getElementById('diff-panel');
  const diffOutput = document.getElementById('diff-output');
  const dirtyBadge = document.getElementById('dirty-badge');
  const headerProfileStatus = document.getElementById('header-profile-status');
  const changeBar = document.getElementById('change-bar');
  const changeCount = document.getElementById('change-count');
  const changeReviewBtn = document.getElementById('change-review');
  const changeDiscardBtn = document.getElementById('change-discard');
  const changeReviewDialog = document.getElementById('change-review-dialog');
  const changeReviewCloseBtn = document.getElementById('change-review-close');
  const changeReviewOutput = document.getElementById('change-review-output');
  const connectionPill = document.getElementById('connection-pill');
  const connectionBanner = document.getElementById('connection-banner');
  const transportLaneChip = document.getElementById('transport-lane-chip');
  const contractQualityChip = document.getElementById('contract-quality-chip');
  const connectFailHelp = document.getElementById('connect-fail-help');
  const headerStatus = document.getElementById('header-status');
  const exportPresetBtn = document.getElementById('export-preset');
  const importPresetBtn = document.getElementById('import-preset');
  const presetPicker = document.getElementById('preset-picker');
  const applySaveProfileBtn = document.getElementById('apply-save-profile');
  const simulatorToggle = document.getElementById('simulator-toggle');
  const emptySimulatorBtn = document.getElementById('empty-start-simulator');
  const usbMidiToggleBtn = document.getElementById('usb-midi-toggle');
  const usbMidiTestBtn = document.getElementById('usb-midi-test');
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
  const performanceTabButtons = Array.from(document.querySelectorAll('[data-performance-tab]'));
  const performancePanels = Array.from(document.querySelectorAll('[data-performance-panel]'));
  const performancePanelHost = document.getElementById('profile-performance-panels');
  performancePanels.forEach((panel) => {
    const tab = panel.dataset.performancePanel;
    panel.id ||= `performance-panel-${tab}`;
    panel.setAttribute('role', 'tabpanel');
    performancePanelHost?.append(panel);
  });
  performanceTabButtons.forEach((button, index) => {
    const tab = button.dataset.performanceTab;
    const panel = performancePanels.find((candidate) => candidate.dataset.performancePanel === tab);
    button.id ||= `performance-tab-${tab || index}`;
    button.setAttribute('role', 'tab');
    if (panel) {
      button.setAttribute('aria-controls', panel.id);
      panel.setAttribute('aria-labelledby', button.id);
    }
    button.addEventListener('keydown', (event) => {
      if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
      const current = Math.max(0, performanceTabButtons.indexOf(button));
      const nextIndex =
        event.key === 'Home'
          ? 0
          : event.key === 'End'
            ? performanceTabButtons.length - 1
            : (current + (event.key === 'ArrowRight' ? 1 : -1) + performanceTabButtons.length) %
              performanceTabButtons.length;
      event.preventDefault();
      performanceTabButtons[nextIndex].focus();
      performanceTabButtons[nextIndex].click();
    });
  });
  const setPerformanceTab = (tab) => {
    performanceTabButtons.forEach((button) => {
      const selected = button.dataset.performanceTab === tab;
      button.setAttribute('aria-pressed', selected ? 'true' : 'false');
      button.setAttribute('aria-selected', selected ? 'true' : 'false');
      button.tabIndex = selected ? 0 : -1;
    });
    performancePanels.forEach((panel) => {
      const visible = panel.dataset.performancePanel === tab;
      panel.classList.toggle('performance-panel-active', visible);
      panel.toggleAttribute('hidden', !visible);
    });
  };
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
  const slotDetailLfo = document.getElementById('slot-detail-lfo');
  const slotDetailValue = document.getElementById('slot-detail-value');
  const deviceMonitor = document.getElementById('device-monitor');
  const powerSafetyPill = document.getElementById('power-safety-pill');
  const globalPowerWarning = document.getElementById('global-power-warning');
  const performerPanel = document.getElementById('performer-panel');
  const stageConnectBtn = document.getElementById('stage-connect');
  const stageConnectionState = document.getElementById('stage-connection-state');
  const stageTelemetryState = document.getElementById('stage-telemetry-state');
  const stageDirtyState = document.getElementById('stage-dirty-state');
  const stageDeviceName = document.getElementById('stage-device-name');
  const stageFwVersion = document.getElementById('stage-fw-version');
  const stageProfileSummary = document.getElementById('stage-profile-summary');
  const stageSceneSummary = document.getElementById('stage-scene-summary');
  const stageClockState = document.getElementById('stage-clock-state');
  const stageMidiOutput = document.getElementById('stage-midi-output');
  const stageSlotFocus = document.getElementById('stage-slot-focus');
  const stagePowerSummary = document.getElementById('stage-power-summary');
  const stageProfileSelect = document.getElementById('stage-profile-select');
  const stageProfileLoadBtn = document.getElementById('stage-profile-load');
  const stageSceneSelect = document.getElementById('stage-scene-select');
  const stageSceneRecallBtn = document.getElementById('stage-scene-recall');
  const stageDraftBlockedNotice = document.getElementById('stage-draft-blocked');
  const stagePanicHelpBtn = document.getElementById('stage-panic-help');
  const stageSlotGrid = document.getElementById('stage-slots');
  const stageEnvelopeContainer = document.getElementById('stage-envelopes');
  const stageEnvelopeCount = document.getElementById('stage-envelope-count');
  const stageMotion = document.getElementById('stage-motion');
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
  const profileNameInput = document.getElementById('profile-name');
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
  const arpPatternLengthInput = document.getElementById('arp-pattern-length');
  const arpAssignmentSlotSelect = document.getElementById('arp-assignment-slot');
  const arpAssignmentAddBtn = document.getElementById('arp-assignment-add');
  const arpAssignmentList = document.getElementById('arp-assignment-list');
  const liveArpSlotInput = document.getElementById('live-arp-slot');
  const liveArpLengthInput = document.getElementById('live-arp-length');
  const liveArpShapeSelect = document.getElementById('live-arp-shape');
  const liveArpSwingInput = document.getElementById('live-arp-swing');
  const liveArpGateInput = document.getElementById('live-arp-gate');
  const liveArpOctaveInput = document.getElementById('live-arp-octave');
  const liveArpPatternLengthInput = document.getElementById('live-arp-pattern-length');
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
  const modMatrixConflictFilter = document.getElementById('mod-matrix-filter-conflicts');
  const modMatrixLfoFilter = document.getElementById('mod-matrix-filter-lfo');
  const modMatrixSlotFilter = document.getElementById('mod-matrix-filter-slot');
  const modMatrixActiveFilter = document.getElementById('mod-matrix-filter-active');
  const modMatrixRefreshBtn = document.getElementById('mod-matrix-refresh');
  const modMatrixExportBtn = document.getElementById('mod-matrix-export');
  const modMatrixCopyConflictsBtn = document.getElementById('mod-matrix-copy-conflicts');
  const modMatrixBody = document.getElementById('mod-matrix-body');
  const modMatrixStatusEl = document.getElementById('mod-matrix-status');
  const sceneGrid = document.getElementById('scene-grid');
  const sceneStatusEl = document.getElementById('scene-status');
  const uiModeButtons = Array.from(document.querySelectorAll('[data-ui-mode-btn]'));
  const uiModeHint = document.getElementById('ui-mode-hint');
  const uiModeNodes = Array.from(document.querySelectorAll('[data-ui-modes], [data-ui-tier]'));
  const advancedTierNodes = Array.from(document.querySelectorAll('[data-ui-tier="advanced"]'));
  const UI_MODE_HINTS = {
    stage:
      'Stage mode keeps only stage-safe connect, profile, scene, power, slot, envelope, and panic controls.',
    basic:
      'Configure keeps everyday slot mapping, profile, import/export, and Apply controls visible.',
    advanced:
      'Lab opens diagnostics, live firmware lanes, modulation tools, and recovery extras.'
  };
  const GLOSSARY = {
    mapping:
      'Knob to MIDI mapping: choose the message type, channel, and number your synth or DAW expects.',
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
      docRoot,
      changeBar,
      changeCount,
      changeDiscardBtn
    }
  });
  changeReviewBtn?.addEventListener('click', () => {
    diffStatusController.renderReview(changeReviewOutput, {
      onNavigate: (path) => focusChangedPath(path)
    });
    changeReviewDialog?.showModal?.();
    changeReviewCloseBtn?.focus();
  });
  changeReviewCloseBtn?.addEventListener('click', () => changeReviewDialog?.close?.());
  changeDiscardBtn?.addEventListener('click', async () => {
    if (
      diffStatusController.shouldConfirmDiscard() &&
      !window.confirm('Discard this staged draft? These changes cannot be recovered from the device.')
    ) {
      return;
    }
    changeDiscardBtn.disabled = true;
    try {
      // A debounced field patch must not resurrect the draft after the operator
      // has explicitly discarded it.
      formRenderer.clearPendingPatches();
      await runtime.rollback();
    } catch (err) {
      setStatus('err', 'Discard failed', err.message || String(err));
      diffStatusController.markDirty(Boolean(runtime.getState().dirty));
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

  function confirmReplaceStaged(actionLabel = 'Continue') {
    if (!runtime.getState().dirty) return true;
    setStatus(
      'warn',
      'Draft protected',
      `${actionLabel} could replace staged work. Apply staged changes or Discard draft first.`
    );
    changeBar?.focus?.();
    return false;
  }
  sessionLogController.bind();
  panicHelpController.bind();

  const deviceMonitorController = createDeviceMonitorController({
    container: deviceMonitor,
    resolveDeviceName
  });
  const powerSafetySummary = createPowerSafetySummary({
    containers: [powerSafetyPill, stagePowerSummary],
    warningContainers: [globalPowerWarning]
  });
  let profileNames = ['', '', '', ''];

  function publishBridgeDisplayMetadata() {
    const state = runtime.getState();
    const slots = Array.isArray(state?.staged?.slots)
      ? state.staged.slots.flatMap((slot, index) => {
          const label = typeof slot?.label === 'string' ? slot.label.trim() : '';
          return label ? [{ index, label }] : [];
        })
      : [];
    Promise.resolve(
      runtime.publishBridgeDisplayMetadata?.({
        profileLabels: profileNames,
        activeProfile: deviceActiveProfileSlot,
        slots
      })
    ).catch(() => {});
  }
  let sceneStates = Array.from({ length: 6 }, () => ({ name: '', available: false }));
  let deviceActiveProfileSlot = null;
  let lastRecalledScene = null;
  const performerPanelController = createPerformerPanelController({
    runtime,
    localManifest,
    resolveDeviceName,
    resolveFirmwareVersion,
    slotTypeAbbreviations: SLOT_TYPE_ABBREVIATIONS,
    connect: () => connectBtn?.click(),
    getConnectionStage: () => connectionPill?.dataset.stage || 'disconnected',
    getConnectionText: () => connectionPill?.textContent || 'Disconnected',
    getActiveProfileSlot: () => {
      const activeButton = profileSlotButtons.find(
        (button) => button.getAttribute('aria-pressed') === 'true'
      );
      return Number(activeButton?.dataset.profileSlot ?? 0);
    },
    getDeviceActiveProfileSlot: () => deviceActiveProfileSlot,
    getProfileName: (slot) => profileNames[Number(slot)] ?? '',
    getSceneState: (slot) => sceneStates[Number(slot)] ?? null,
    getLastRecalledScene: () => lastRecalledScene,
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
      telemetryState: stageTelemetryState,
      dirtyState: stageDirtyState,
      deviceName: stageDeviceName,
      fwVersion: stageFwVersion,
      profileSummary: stageProfileSummary,
      sceneSummary: stageSceneSummary,
      clockState: stageClockState,
      midiOutput: stageMidiOutput,
      slotFocus: stageSlotFocus,
      profileSelect: stageProfileSelect,
      profileLoadBtn: stageProfileLoadBtn,
      sceneSelect: stageSceneSelect,
      sceneRecallBtn: stageSceneRecallBtn,
      draftBlockedNotice: stageDraftBlockedNotice,
      panicHelpBtn: stagePanicHelpBtn,
      slotGrid: stageSlotGrid,
      envelopeContainer: stageEnvelopeContainer,
      envelopeCount: stageEnvelopeCount
    }
  });
  const transportToolbarController = createTransportToolbarController({
    runtime,
    runtimeOptions,
    resolveDeviceName,
    resolveFirmwareVersion,
    setStatus,
    syncConfigFileButtons,
    canReplaceStaged: confirmReplaceStaged,
    onConnectionPillChanged: updateStagePanel,
    elements: {
      docRoot,
      connectBtn,
      checkCompatibilityBtn,
      configModeBtn,
      applyBtn,
      simulatorToggle,
      emptySimulatorBtn,
      connectionPill,
      connectionBanner,
      transportLaneChip,
      contractQualityChip,
      connectFailHelp,
      usbMidiToggleBtn,
      usbMidiTestBtn,
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
    setPerformerVisible: (visible) => {
      performerPanelController.setVisible(visible);
      if (!visible && stageMotion?.open) stageMotion.open = false;
    },
    onModeChanged: updateStagePanel,
    elements: {
      uiModeButtons,
      uiModeHint,
      uiModeNodes,
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
      if (docRoot) docRoot.dataset.importedDraft = 'true';
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
    confirmReplaceStaged,
    onImportedDraft: () => {
      if (docRoot) docRoot.dataset.importedDraft = 'true';
    },
    onProfileNamesChanged: (names) => {
      profileNames = Array.isArray(names) ? [...names] : profileNames;
      updateStagePanel();
      publishBridgeDisplayMetadata();
    },
    onScenesChanged: (scenes) => {
      sceneStates = Array.isArray(scenes) ? scenes.map((scene) => ({ ...scene })) : sceneStates;
      updateStagePanel();
    },
    onDeviceActiveProfileChanged: (slot) => {
      const next = slot !== null && slot !== undefined && Number.isInteger(Number(slot))
        ? Number(slot)
        : null;
      if (next !== deviceActiveProfileSlot) lastRecalledScene = null;
      deviceActiveProfileSlot = next;
      updateStagePanel();
      publishBridgeDisplayMetadata();
    },
    onSceneRecalled: (slot, scene) => {
      lastRecalledScene = {
        slot: Number(slot),
        name: typeof scene?.name === 'string' ? scene.name : ''
      };
      updateStagePanel();
    },
    elements: {
      profileSlotButtons,
      profileSlotStatus,
      profileNameInput,
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
      arpPatternLengthInput,
      arpAssignmentSlotSelect,
      arpAssignmentAddBtn,
      arpAssignmentList,
      liveArpSlotInput,
      liveArpLengthInput,
      liveArpShapeSelect,
      liveArpSwingInput,
      liveArpGateInput,
      liveArpOctaveInput,
      liveArpPatternLengthInput,
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
      modMatrixConflictFilter,
      modMatrixLfoFilter,
      modMatrixSlotFilter,
      modMatrixActiveFilter,
      modMatrixRefreshBtn,
      modMatrixExportBtn,
      modMatrixCopyConflictsBtn,
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
      slotDetailLfo,
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
    getEditorTab: () => uiModeController.getEditorTab(),
    openLabTab: (tab) => {
      uiModeController.setUIMode('advanced');
      uiModeController.setEditorTab(tab);
    }
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
    onTelemetryPainted: () => populateDetail({ renderEditor: false }),
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
  performanceTabButtons.forEach((button) => {
    button.addEventListener('click', () => setPerformanceTab(button.dataset.performanceTab));
  });
  uiModeController.setUIMode(initialUiMode, { persist: Boolean(requestedUiMode) });
  uiModeController.setEditorTab(uiModeController.getEditorTab());
  uiModeController.setUtilityTab('console');
  setPerformanceTab('arp');

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
    profileMacroScenePanel.onTelemetry(frame);
  });
  runtime.on('telemetry-health', (health) => {
    deviceMonitorController.renderTelemetryHealth(health);
    updateStagePanel();
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
    formRenderer.updateValues();
    syncConfigFileButtons();
    profileMacroScenePanel.onConfigChanged();
    updateStagePanel();
    panicHelpController.render();
    publishBridgeDisplayMetadata();
  });
  runtime.on('config-transaction', ({ state, deviceAuthority }) => {
    diffStatusController.setTransactionState(deviceAuthority || state);
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
  runtime.on('contract-quality', ({ quality, applyAllowed }) => {
    transportToolbarController.onContractQuality();
    if (!applyAllowed && applyBtn) applyBtn.disabled = true;
    if (!applyAllowed) {
      setStatus('warn', 'Degraded device contract', `Apply is blocked: ${quality}.`);
    }
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
    if (docRoot) delete docRoot.dataset.importedDraft;
    if (runtime.getState().dirty) {
      diffStatusController.setStatus(
        'ok',
        'Applied',
        'Applied the captured configuration. Newer edits remain staged.'
      );
    } else {
      diffStatusController.clearApplied(checksum);
    }
    sessionLogController.recordEvent(
      'APPLY',
      'Device acknowledged staged edits',
      `Checksum ${String(checksum).slice(0, 8)}...`,
      'ok'
    );
    panicHelpController.render();
  });
  runtime.on('apply-uncertain', ({ reason }) => {
    retryReadbackBtn?.removeAttribute('hidden');
    setStatus('warn', 'Apply outcome uncertain', 'Waiting for authoritative device configuration readback.');
    sessionLogController.recordEvent('APPLY', 'Outcome uncertain', String(reason ?? 'unknown'), 'warn');
    panicHelpController.render();
  });
  runtime.on('resynchronized', () => {
    retryReadbackBtn?.setAttribute('hidden', '');
    setStatus('ok', 'Resynchronized', 'Browser state now matches the controller readback.');
    sessionLogController.recordEvent('APPLY', 'Resynchronized from device', '', 'ok');
    panicHelpController.render();
  });
  runtime.on('config-conflict', ({ conflicts }) => {
    const count = Array.isArray(conflicts) ? conflicts.length : 0;
    setStatus(
      'warn',
      'Device edit conflicts with staged work',
      `${count} field${count === 1 ? '' : 's'} kept as staged; review before Apply.`
    );
    sessionLogController.recordEvent('CONFIG', 'Device/staged conflict', `${count} leaf field(s)`, 'warn');
    panicHelpController.render();
  });
  runtime.on('device-config-patch', ({ patch, conflicts, previous, live }) => {
    const detail = describeDeviceConfigPatch(patch, previous, live);
    const hasConflicts = Array.isArray(conflicts) && conflicts.length > 0;
    setStatus(
      hasConflicts ? 'warn' : 'ok',
      'Device reported',
      hasConflicts
        ? `${detail}. Local conflicting fields remain staged for review.`
        : `${detail}. Editor controls now follow device truth.`
    );
    sessionLogController.recordEvent(
      'CONFIG',
      'Device-reported configuration change',
      detail,
      hasConflicts ? 'warn' : 'ok'
    );
  });
  retryReadbackBtn?.addEventListener('click', async () => {
    retryReadbackBtn.disabled = true;
    try {
      const result = await runtime.resynchronize();
      if (!result) throw new Error('Device readback is still unavailable. Reconnect and retry.');
      setStatus('ok', 'Resynchronized', 'Browser state now matches the controller readback.');
      retryReadbackBtn.setAttribute('hidden', '');
    } catch (err) {
      setStatus('warn', 'Readback still unresolved', err.message || String(err));
    } finally {
      retryReadbackBtn.disabled = false;
    }
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
        ? 'An adapter is registered, but automatic migration is not yet implemented. Export the preset and use a matching App/firmware pair.'
        : 'Export your preset and update firmware/UI to continue.'
    }`;
    if (migrationApply) migrationApply.disabled = true;
    migrationDialog.showModal();
  });
  runtime.on('snapshot-restore-required', ({ snapshot }) => {
    const savedAt = snapshot?.saved_at || (snapshot?.timestamp ? new Date(snapshot.timestamp).toISOString() : 'unknown time');
    const firmware = snapshot?.device?.firmware_git_sha || 'unknown firmware';
    const restore = window.confirm(
      `A staged workspace from ${savedAt} targets ${firmware}, not this firmware. Restore it for review? It will remain staged and will not be applied automatically.`
    );
    if (restore) {
      runtime.restoreLocalState({ allowDifferentFirmware: true });
      setStatus('warn', 'Workspace restored for review', `Origin: ${firmware} • saved ${savedAt}`);
    } else {
      runtime.discardSavedWorkspace();
      setStatus('warn', 'Old workspace discarded', `Origin: ${firmware} • saved ${savedAt}`);
    }
  });
  runtime.on('connected', ({ manifest }) => {
    // Connection flips the entire toolbar/profile surface into interactive mode.
    setConnectionPill('live', 'Connected');
    setConnectionBanner('live', manifest);
    connectFailHelp?.removeAttribute('open');
    if (applyBtn) applyBtn.disabled = true;
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
    updatePowerSafetySummary(manifest, true);
    updateStagePanel();
    panicHelpController.render();
  });
  runtime.on('disconnected', () => {
    // Mirror the connected handler in reverse so stale controls cannot issue RPCs offline.
    setConnectionPill('disconnected', 'Disconnected');
    setConnectionBanner('disconnected', runtime.getState().manifest);
    if (applyBtn) applyBtn.disabled = true;
    syncConfigFileButtons();
    setStatus('warn', 'Disconnected', 'Reconnect to continue editing.');
    sessionLogController.recordEvent('CONNECTION', 'Disconnected', '', 'warn');
    profileMacroScenePanel.onDisconnected();
    transportToolbarController.onDisconnected();
    updatePowerSafetySummary({}, false);
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
    updatePowerSafetySummary({}, false);
    updateStagePanel();
    panicHelpController.render();
  });
  runtime.on('macro', (payload) => profileMacroScenePanel.onMacro(payload));
  runtime.on('scene', (payload) => {
    profileMacroScenePanel.onScene(payload);
    updateStagePanel();
  });
  runtime.on('rollback', () => {
    if (docRoot) delete docRoot.dataset.importedDraft;
    diffStatusController.updateDiff(false);
    diffStatusController.markDirty(false);
    setStatus('warn', 'Rollback', 'Local edits were discarded.');
    panicHelpController.render();
  });

  runtime.restoreLocalState();
  updatePowerSafetySummary({}, false);
  updateStagePanel();
  syncConfigFileButtons();
  panicHelpController.render();
  primeCompatibilityStatus();
  window.addEventListener('beforeunload', (event) => {
    if (!runtime.getState().dirty) return;
    event.preventDefault();
    event.returnValue = '';
  });
  new MidiMonitor({ container: document.getElementById('midi-panel') });
  new ScopePanel({
    container: document.getElementById('scope-panel'),
    runtime,
    manifest: localManifest
  });
  new ScopePanel({
    container: document.getElementById('stage-motion-panel'),
    runtime,
    manifest: localManifest,
    renderToggle: stageMotion
  });
  if (connectBtn) {
    connectBtn.disabled = false;
    connectBtn.removeAttribute('aria-busy');
  }
  if (stageConnectBtn) {
    stageConnectBtn.disabled = false;
    stageConnectBtn.removeAttribute('aria-busy');
  }
  if (docRoot) docRoot.dataset.mn42Ready = 'true';

  function updatePowerSafetySummary(
    manifest,
    connected = connectionPill?.dataset.stage === 'live'
  ) {
    powerSafetySummary.render(manifest, { connected });
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

  // A manifest establishes device identity, not a write-ready session. Only
  // the final connected event may promote the UI to live.
  function updateHeaderManifest(manifest) {
    if (connectionPill?.dataset.stage !== 'live') {
      setConnectionPill('handshake', 'Manifest received');
      setConnectionBanner('handshake', manifest);
    }
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
    if (headerProfileStatus) {
      const connected = connectionPill?.dataset.stage === 'live';
      const active = deviceActiveProfileSlot;
      if (connected && Number.isInteger(active)) {
        const name = profileNames[active]?.trim?.() ?? '';
        const fallback = `Profile ${performerPanelController.slotLabel(active)}`;
        headerProfileStatus.textContent = name ? `${name} · ${fallback}` : fallback;
      } else {
        const target = Number(stageProfileSelect?.value ?? 0);
        const name = profileNames[target]?.trim?.() ?? '';
        const fallback = `Profile ${performerPanelController.slotLabel(target)}`;
        headerProfileStatus.textContent = `${name ? `${name} · ${fallback}` : fallback} target`;
      }
    }
  }

  // Change which slot is focused in the inspector/editor pane.
  function selectSlot(index) {
    slotWorkspaceController.selectSlot(index);
  }

  function focusChangedPath(path) {
    const slotMatch = String(path ?? '').match(/^slots[.\[]+(\d+)/);
    if (slotMatch) {
      selectSlot(Number(slotMatch[1]));
      changeReviewDialog?.close?.();
      document.getElementById('editor-panel')?.scrollIntoView?.({ block: 'start' });
      return;
    }
    const subsystem = String(path ?? '').split(/[.[]/, 1)[0];
    const target = document.querySelector(`[data-schema-section="${subsystem}"]`);
    if (target) {
      changeReviewDialog?.close?.();
      target.scrollIntoView?.({ block: 'center' });
      target.querySelector('input, select, button')?.focus?.();
    }
  }

  // Fill the slot detail card from the selected slot plus latest telemetry.
  function populateDetail(options) {
    profileMacroScenePanel.setLiveArpSlot(slotState.selected);
    slotEditorPanel.populateDetail(options);
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
