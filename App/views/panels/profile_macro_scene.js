import {
  clampProfileSlot,
  persistProfileSlot,
  readProfileSlotPreference
} from '../state/ui_preferences.js';
import { createProfileFileIO } from './profile_file_io.js';
import { createProfileSceneControls } from './profile_scene_controls.js';
import {
  createProfileWorkflow,
  resolveProfileCapabilities,
  supportsAnyProfileAction as supportsAnyProfileActionForCapabilities,
  supportsGuidedProfileFlow as supportsGuidedProfileFlowForCapabilities
} from './profile_workflow.js';

const PROFILE_LABELS = ['A', 'B', 'C', 'D'];
const PROFILE_RPC_TIMEOUT_MS = 30000;
const MACRO_SAVE_COMMAND = 'SAVE_MACRO_SLOT';
const MACRO_RECALL_COMMAND = 'RECALL_MACRO_SLOT';
const SCENE_SLOT_COUNT = 6;
const ARP_SHAPE_OPTIONS = ['Up', 'Down', 'Up/Down', 'Random', 'Drunk', 'Euclidean'];
const LFO_SHAPE_OPTIONS = ['Sine', 'Triangle', 'Saw', 'Square', 'Sample & Hold', 'Random Slew'];
const LFO_SYNC_RATIO_OPTIONS = ['1/1', '1/2', '1/4', '1/8', '1/16', '1/32', 'x2', 'x4'];
const LFO_ROUTE_TYPE_OPTIONS = ['Internal', 'MIDI CC 7-bit', 'MIDI CC 14-bit', 'OSC', 'Slot'];
const LFO_INTERNAL_TARGET_OPTIONS = [
  'EF Gain Trim',
  'Arp Swing',
  'Velocity Shift',
  'Note Chance',
  'Arp Gate',
  'Jitter Depth',
  'Jitter Smoothness'
];
const PROFILE_LFO_COUNT = 2;
const PROFILE_MAX_ROUTES = 8;
const SLOT_COUNT = 42;
const MOD_MATRIX_ROUTE_PREVIEW_LIMIT = 80;

function createDefaultArpDraft() {
  return {
    length_ticks: 12,
    shape: 0,
    swing_percent: 0,
    gate_percent: 50,
    octave_range: 0
  };
}

function clampInteger(value, min, max, fallback) {
  const numeric = Number(value);
  if (!Number.isFinite(numeric)) return fallback;
  return Math.min(max, Math.max(min, Math.round(numeric)));
}

function clampFloat(value, min, max, fallback, precision = 3) {
  const numeric = Number(value);
  if (!Number.isFinite(numeric)) return fallback;
  const clamped = Math.min(max, Math.max(min, numeric));
  return Number(clamped.toFixed(precision));
}

function createDefaultLfoEntry(index) {
  if (index === 1) {
    return {
      index,
      shape: 1,
      frequency_hz: 0.5,
      depth: 0,
      bipolar: true,
      sync: false,
      sync_ratio: 0
    };
  }
  return {
    index,
    shape: 0,
    frequency_hz: 1,
    depth: 0,
    bipolar: false,
    sync: false,
    sync_ratio: 0
  };
}

function createDefaultRouteDrafts() {
  return [
    { type: 0, lfo: 0, depth: 1, target: 1, channel: 1, cc_msb: 0, cc_lsb: 32 },
    { type: 0, lfo: 1, depth: 1, target: 0, channel: 1, cc_msb: 0, cc_lsb: 32 },
    { type: 0, lfo: 1, depth: 0.5, target: 2, channel: 1, cc_msb: 0, cc_lsb: 32 }
  ];
}

function createDefaultLfoDraft() {
  return {
    lfos: Array.from({ length: PROFILE_LFO_COUNT }, (_, index) => createDefaultLfoEntry(index)),
    routes: createDefaultRouteDrafts()
  };
}

function normalizeLfoEntry(entry, index) {
  const fallback = createDefaultLfoEntry(index);
  return {
    index,
    shape: clampInteger(entry?.shape, 0, LFO_SHAPE_OPTIONS.length - 1, fallback.shape),
    frequency_hz: clampFloat(entry?.frequency_hz, 0.01, 100, fallback.frequency_hz, 2),
    depth: clampFloat(entry?.depth, 0, 1, fallback.depth, 3),
    bipolar: Boolean(entry?.bipolar),
    sync: Boolean(entry?.sync),
    sync_ratio: clampInteger(
      entry?.sync_ratio,
      0,
      LFO_SYNC_RATIO_OPTIONS.length - 1,
      fallback.sync_ratio
    )
  };
}

function normalizeRouteEntry(entry = {}) {
  const type = clampInteger(entry.type, 0, LFO_ROUTE_TYPE_OPTIONS.length - 1, 0);
  const slot = clampInteger(entry.slot ?? entry.target, 0, SLOT_COUNT - 1, 0);
  const minValue = clampInteger(entry.min ?? entry.minValue, 0, 127, 0);
  const maxValue = clampInteger(entry.max ?? entry.maxValue, 0, 127, 127);
  return {
    type,
    lfo: clampInteger(entry.lfo, 0, PROFILE_LFO_COUNT - 1, 0),
    depth: clampFloat(entry.depth, 0, 1, 1, 3),
    amount: clampInteger(entry.amount, -100, 100, 100),
    min: Math.min(minValue, maxValue),
    max: Math.max(minValue, maxValue),
    target:
      type === 4 ? slot : clampInteger(entry.target, 0, LFO_INTERNAL_TARGET_OPTIONS.length - 1, 0),
    slot,
    channel: clampInteger(entry.channel, 1, 16, 1),
    cc_msb: clampInteger(entry.cc_msb, 0, 127, 0),
    cc_lsb: clampInteger(entry.cc_lsb, 0, 127, 32)
  };
}

export function createProfileMacroScenePanel({
  runtime,
  formRenderer,
  localManifest,
  setStatus,
  elements = {},
  getSelectedSlot = () => 0
} = {}) {
  const {
    profileSlotButtons = [],
    profileSlotStatus = null,
    profileSaveBtn = null,
    profileLoadBtn = null,
    profileResetBtn = null,
    profileWizardTarget = null,
    profileWizardSwitchBtn = null,
    profileWizardApplyBtn = null,
    profileWizardSaveBtn = null,
    profileWizardStatus = null,
    profileDownloadBtn = null,
    profileUploadBtn = null,
    applySaveProfileBtn = null,
    profileHint = null,
    macroSaveBtn = null,
    macroRecallBtn = null,
    macroStatusEl = null,
    arpCard = null,
    arpRefreshBtn = null,
    arpSaveBtn = null,
    arpStatusEl = null,
    arpLengthInput = null,
    arpShapeSelect = null,
    arpSwingInput = null,
    arpGateInput = null,
    arpOctaveInput = null,
    liveArpSlotInput = null,
    liveArpLengthInput = null,
    liveArpShapeSelect = null,
    liveArpSwingInput = null,
    liveArpGateInput = null,
    liveArpOctaveInput = null,
    liveArpRefreshBtn = null,
    liveArpApplyBtn = null,
    liveArpStartBtn = null,
    liveArpStopBtn = null,
    liveArpStatusEl = null,
    lfoCard = null,
    lfoEditor = null,
    lfoRouteAddBtn = null,
    lfoRoutesClearBtn = null,
    lfoRefreshBtn = null,
    lfoSaveBtn = null,
    lfoStatusEl = null,
    modMatrixRefreshBtn = null,
    modMatrixBody = null,
    modMatrixStatusEl = null,
    sceneGrid = null,
    sceneStatusEl = null
  } = elements;

  let profileInteractable = false;
  let profileWizardBusy = false;
  let macroBusy = false;
  let macroAvailable = false;
  let arpBusy = false;
  let liveArpBusy = false;
  let liveArpDraft = {
    active: false,
    slot: 0,
    length_ticks: 12,
    shape: 0,
    swing_percent: 0,
    gate_percent: 50,
    octave_range: 0,
    pattern_length: 4
  };
  let liveArpSlotFollowsSelection = true;
  let lfoBusy = false;
  let modMatrixBusy = false;
  let modMatrixReport = null;
  let arpDraft = createDefaultArpDraft();
  let lfoDraft = createDefaultLfoDraft();
  let lfoDraftDirty = false;
  let selectedLfoRouteIndex = null;
  let activeProfileSlot = readProfileSlotPreference({ slotCount: PROFILE_LABELS.length });
  let deviceActiveProfileSlot = activeProfileSlot;
  let profileWizardTargetSlot = activeProfileSlot;
  let deviceCapabilities = resolveProfileCapabilities(localManifest);
  let bound = false;
  const profileFileIO = createProfileFileIO({
    runtime,
    setStatus,
    clampSlot: clampProfileSlot,
    slotCount: PROFILE_LABELS.length,
    getActiveProfileSlot: () => activeProfileSlot,
    slotLabel,
    describeSlot,
    setActiveProfileSlot
  });
  const sceneControls = createProfileSceneControls({
    runtime,
    setStatus,
    sceneGrid,
    sceneStatusEl,
    sceneSlotCount: SCENE_SLOT_COUNT,
    isInteractable: () => profileInteractable,
    supportsScenes: () => deviceCapabilities.scenes
  });
  const profileWorkflow = createProfileWorkflow({
    runtime,
    formRenderer,
    setStatus,
    clampSlot: clampProfileSlot,
    slotCount: PROFILE_LABELS.length,
    getCapabilities: () => deviceCapabilities,
    isInteractable: () => profileInteractable,
    getActiveProfileSlot: () => activeProfileSlot,
    setActiveProfileSlot,
    describeSlot,
    refreshProfileControls,
    refreshProfileUtilities,
    timeoutMs: PROFILE_RPC_TIMEOUT_MS
  });
  const runProfileRpc = (...args) => profileWorkflow.runProfileRpc(...args);

  // Some UI paths only need to know whether any device-backed profile action exists.
  function supportsAnyProfileAction() {
    return supportsAnyProfileActionForCapabilities(deviceCapabilities);
  }

  // Guided save/switch flows only make sense when both load and save are exposed.
  function supportsGuidedProfileFlow() {
    return supportsGuidedProfileFlowForCapabilities(deviceCapabilities);
  }

  // Explain whether the selected profile slot reflects real device storage or only
  // the browser's chosen target.
  function profileSlotModeCopy() {
    if (!supportsAnyProfileAction()) return 'local target';
    if (deviceActiveProfileSlot === activeProfileSlot) return 'active on board';
    return `editing target • board active ${slotLabel(deviceActiveProfileSlot)}`;
  }

  // Keep the recovery/profile hint text honest as capabilities and connectivity change.
  function updateProfileHint() {
    if (!profileHint) return;
    if (!profileInteractable) {
      profileHint.textContent =
        'Download/upload always works as a file backup. Connect to see whether this firmware exposes device-backed profile actions.';
      return;
    }
    if (supportsAnyProfileAction()) {
      profileHint.textContent =
        'Device-backed profile actions are enabled for this firmware. Download/upload remains the safest file backup.';
      return;
    }
    profileHint.textContent =
      'This firmware does not expose browser-driven profile save, switch, or reset yet. Use Download/Upload for file backups.';
  }

  function setArpStatus(state, message) {
    if (!arpStatusEl) return;
    arpStatusEl.dataset.state = state;
    arpStatusEl.textContent = message;
  }

  function setLiveArpStatus(state, message) {
    if (!liveArpStatusEl) return;
    liveArpStatusEl.dataset.state = state;
    liveArpStatusEl.textContent = message;
  }

  function setLfoStatus(state, message) {
    if (!lfoStatusEl) return;
    lfoStatusEl.dataset.state = state;
    lfoStatusEl.textContent = message;
  }

  function setModMatrixStatus(state, message) {
    if (!modMatrixStatusEl) return;
    modMatrixStatusEl.dataset.state = state;
    modMatrixStatusEl.textContent = message;
  }

  function formatRouteRange(route) {
    const min = route?.range?.min;
    const max = route?.range?.max;
    if (Number.isFinite(Number(min)) && Number.isFinite(Number(max))) {
      return `${min}-${max}`;
    }
    return '-';
  }

  function formatRouteDepth(route) {
    if (Number.isFinite(Number(route?.depth))) return Number(route.depth).toFixed(2);
    if (Number.isFinite(Number(route?.amount))) return Number(route.amount).toFixed(2);
    return '-';
  }

  function formatRouteLastValue(route) {
    if (Number.isFinite(Number(route?.last_value))) return String(route.last_value);
    return '-';
  }

  function clearSelectedLfoRoute() {
    selectedLfoRouteIndex = null;
  }

  function readMatrixRouteIndex(route = {}) {
    const idMatch = /(?:^|_)route(\d+)$/.exec(String(route.id ?? ''));
    if (idMatch) return Number(idMatch[1]);
    return null;
  }

  function isLfoMatrixRoute(route = {}) {
    if (route?.source_type === 'lfo') return true;
    return /^lfo\d+$/i.test(String(route?.source ?? ''));
  }

  function canSelectMatrixRoute() {
    return !lfoDraftDirty && activeProfileSlot === deviceActiveProfileSlot;
  }

  function focusSelectedLfoRouteCard() {
    if (!lfoEditor || selectedLfoRouteIndex === null) return;
    const card = lfoEditor.querySelector(`[data-route-index="${selectedLfoRouteIndex}"]`);
    if (!card) return;
    card.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
    if (typeof card.focus === 'function') {
      card.focus({ preventScroll: true });
    }
  }

  function handleModMatrixRouteSelection(route = {}) {
    if (!isLfoMatrixRoute(route)) return;
    const routeIndex = readMatrixRouteIndex(route);
    if (routeIndex === null || routeIndex < 0 || routeIndex >= lfoDraft.routes.length) {
      setModMatrixStatus('busy', 'Reload slot LFOs to inspect this matrix row.');
      return;
    }
    if (!canSelectMatrixRoute()) {
      setModMatrixStatus(
        'busy',
        activeProfileSlot !== deviceActiveProfileSlot
          ? `Matrix reflects ${describeBoardActiveSlot()}. Switch to that slot to inspect matching LFO routes.`
          : 'Push & save or reload the slot before using matrix row selection.'
      );
      return;
    }
    selectedLfoRouteIndex = routeIndex;
    renderLfoEditor();
    renderModMatrix();
    focusSelectedLfoRouteCard();
    setModMatrixStatus('ok', `Selected Route ${routeIndex + 1} in ${describeSlot()}.`);
  }

  function renderModMatrix(report = modMatrixReport) {
    if (!modMatrixBody) return;
    modMatrixBody.innerHTML = '';
    const routes = Array.isArray(report?.routes) ? report.routes : [];
    const conflicts = Array.isArray(report?.conflicts) ? report.conflicts : [];
    const visibleRoutes = routes.slice(0, MOD_MATRIX_ROUTE_PREVIEW_LIMIT);
    if (!report) {
      const empty = document.createElement('p');
      empty.className = 'lfo-empty';
      empty.textContent = 'No modulation matrix loaded.';
      modMatrixBody.appendChild(empty);
      return;
    }

    const summary = document.createElement('div');
    summary.className = 'mod-matrix-summary';
    [`Routes ${routes.length}`, `Conflicts ${conflicts.length}`].forEach((text) => {
      const item = document.createElement('span');
      item.textContent = text;
      summary.appendChild(item);
    });
    if (routes.length > MOD_MATRIX_ROUTE_PREVIEW_LIMIT) {
      const item = document.createElement('span');
      item.textContent = `Showing ${visibleRoutes.length} of ${routes.length}`;
      summary.appendChild(item);
    }
    modMatrixBody.appendChild(summary);

    if (conflicts.length) {
      const list = document.createElement('ul');
      list.className = 'mod-matrix-conflicts';
      conflicts.forEach((conflict) => {
        const item = document.createElement('li');
        item.textContent = conflict.message ?? `${conflict.target ?? 'target'} collision`;
        list.appendChild(item);
      });
      modMatrixBody.appendChild(list);
    }

    const tableWrap = document.createElement('div');
    tableWrap.className = 'mod-matrix-table-wrap';
    const table = document.createElement('table');
    table.className = 'mod-matrix-table';
    const thead = document.createElement('thead');
    const headRow = document.createElement('tr');
    ['Source', 'Transform', 'Destination', 'Depth', 'Range', 'Active', 'Last'].forEach((label) => {
      const th = document.createElement('th');
      th.textContent = label;
      headRow.appendChild(th);
    });
    thead.appendChild(headRow);
    table.appendChild(thead);

    const tbody = document.createElement('tbody');
    visibleRoutes.forEach((route) => {
      const row = document.createElement('tr');
      const routeIndex = readMatrixRouteIndex(route);
      const actionable = isLfoMatrixRoute(route) && routeIndex !== null;
      if (actionable) {
        row.classList.add('mod-matrix-row-actionable');
        row.tabIndex = 0;
        row.setAttribute('role', 'button');
        row.setAttribute('aria-label', `Inspect Route ${routeIndex + 1}`);
        if (selectedLfoRouteIndex === routeIndex) {
          row.dataset.selected = 'true';
        }
        row.addEventListener('click', () => handleModMatrixRouteSelection(route));
        row.addEventListener('keydown', (event) => {
          if (event.key !== 'Enter' && event.key !== ' ') return;
          event.preventDefault();
          handleModMatrixRouteSelection(route);
        });
      }
      [
        route.source ?? '-',
        route.transform ?? route.mode ?? '-',
        route.destination ?? '-',
        formatRouteDepth(route),
        formatRouteRange(route),
        route.active === false ? 'No' : 'Yes',
        formatRouteLastValue(route)
      ].forEach((value) => {
        const td = document.createElement('td');
        td.textContent = String(value);
        row.appendChild(td);
      });
      tbody.appendChild(row);
    });
    table.appendChild(tbody);
    tableWrap.appendChild(table);
    modMatrixBody.appendChild(tableWrap);
    if (routes.length > MOD_MATRIX_ROUTE_PREVIEW_LIMIT) {
      const note = document.createElement('p');
      note.className = 'mod-matrix-limit';
      note.textContent = `Showing ${visibleRoutes.length} of ${routes.length} routes. Refresh after filtering once export support lands.`;
      modMatrixBody.appendChild(note);
    }
  }

  function syncArpForm() {
    if (arpLengthInput) arpLengthInput.value = String(arpDraft.length_ticks);
    if (arpShapeSelect) arpShapeSelect.value = String(arpDraft.shape);
    if (arpSwingInput) arpSwingInput.value = String(arpDraft.swing_percent);
    if (arpGateInput) arpGateInput.value = String(arpDraft.gate_percent);
    if (arpOctaveInput) arpOctaveInput.value = String(arpDraft.octave_range);
  }

  function setArpDraft(nextDraft = {}) {
    arpDraft = {
      length_ticks: clampInteger(nextDraft.length_ticks, 1, 24, arpDraft.length_ticks),
      shape: clampInteger(nextDraft.shape, 0, ARP_SHAPE_OPTIONS.length - 1, arpDraft.shape),
      swing_percent: clampInteger(nextDraft.swing_percent, 0, 80, arpDraft.swing_percent),
      gate_percent: clampInteger(nextDraft.gate_percent, 5, 100, arpDraft.gate_percent),
      octave_range: clampInteger(nextDraft.octave_range, 0, 3, arpDraft.octave_range)
    };
    syncArpForm();
  }

  function readArpFormIntoDraft() {
    setArpDraft({
      length_ticks: arpLengthInput?.value ?? arpDraft.length_ticks,
      shape: arpShapeSelect?.value ?? arpDraft.shape,
      swing_percent: arpSwingInput?.value ?? arpDraft.swing_percent,
      gate_percent: arpGateInput?.value ?? arpDraft.gate_percent,
      octave_range: arpOctaveInput?.value ?? arpDraft.octave_range
    });
  }

  function syncLiveArpForm() {
    if (liveArpSlotInput) liveArpSlotInput.value = String(liveArpDraft.slot);
    if (liveArpLengthInput) liveArpLengthInput.value = String(liveArpDraft.length_ticks);
    if (liveArpShapeSelect) liveArpShapeSelect.value = String(liveArpDraft.shape);
    if (liveArpSwingInput) liveArpSwingInput.value = String(liveArpDraft.swing_percent);
    if (liveArpGateInput) liveArpGateInput.value = String(liveArpDraft.gate_percent);
    if (liveArpOctaveInput) liveArpOctaveInput.value = String(liveArpDraft.octave_range);
  }

  function setLiveArpDraft(nextDraft = {}) {
    liveArpDraft = {
      active: Boolean(nextDraft.active),
      slot: clampInteger(nextDraft.slot, 0, SLOT_COUNT - 1, liveArpDraft.slot),
      length_ticks: clampInteger(nextDraft.length_ticks, 1, 24, liveArpDraft.length_ticks),
      shape: clampInteger(nextDraft.shape, 0, ARP_SHAPE_OPTIONS.length - 1, liveArpDraft.shape),
      swing_percent: clampInteger(nextDraft.swing_percent, 0, 80, liveArpDraft.swing_percent),
      gate_percent: clampInteger(nextDraft.gate_percent, 5, 100, liveArpDraft.gate_percent),
      octave_range: clampInteger(nextDraft.octave_range, 0, 3, liveArpDraft.octave_range),
      pattern_length: clampInteger(nextDraft.pattern_length, 2, 16, liveArpDraft.pattern_length)
    };
    syncLiveArpForm();
  }

  function setLiveArpSlot(slotIndex = getSelectedSlot()) {
    if (liveArpDraft.active) {
      refreshProfileControls();
      return;
    }
    if (!liveArpSlotFollowsSelection) {
      refreshProfileControls();
      return;
    }
    const nextSlot = clampInteger(slotIndex, 0, SLOT_COUNT - 1, liveArpDraft.slot);
    if (nextSlot === liveArpDraft.slot) return;
    setLiveArpDraft({ ...liveArpDraft, slot: nextSlot });
    setLiveArpStatus('muted', `Selected slot ${nextSlot}. Push or start live arp from here.`);
    refreshProfileControls();
  }

  function readLiveArpFormIntoDraft() {
    const nextSlot = clampInteger(
      liveArpSlotInput?.value ?? liveArpDraft.slot,
      0,
      SLOT_COUNT - 1,
      liveArpDraft.slot
    );
    liveArpSlotFollowsSelection =
      nextSlot === clampInteger(getSelectedSlot(), 0, SLOT_COUNT - 1, 0);
    setLiveArpDraft({
      ...liveArpDraft,
      slot: nextSlot,
      length_ticks: liveArpLengthInput?.value ?? liveArpDraft.length_ticks,
      shape: liveArpShapeSelect?.value ?? liveArpDraft.shape,
      swing_percent: liveArpSwingInput?.value ?? liveArpDraft.swing_percent,
      gate_percent: liveArpGateInput?.value ?? liveArpDraft.gate_percent,
      octave_range: liveArpOctaveInput?.value ?? liveArpDraft.octave_range
    });
  }

  function createLfoControl(labelText, control) {
    const label = document.createElement('label');
    const title = document.createElement('span');
    title.textContent = labelText;
    label.append(title, control);
    return label;
  }

  function createSelect(options, value, onChange) {
    const select = document.createElement('select');
    options.forEach((label, index) => {
      const option = document.createElement('option');
      option.value = String(index);
      option.textContent = label;
      option.selected = index === value;
      select.appendChild(option);
    });
    select.addEventListener('change', () => onChange(select.value));
    return select;
  }

  function createBooleanSelect(value, onChange) {
    const select = document.createElement('select');
    [
      { value: '0', label: 'Off' },
      { value: '1', label: 'On' }
    ].forEach((entry) => {
      const option = document.createElement('option');
      option.value = entry.value;
      option.textContent = entry.label;
      option.selected = Boolean(Number(entry.value)) === Boolean(value);
      select.appendChild(option);
    });
    select.addEventListener('change', () => onChange(select.value === '1'));
    return select;
  }

  function createNumberInput({ value, min, max, step, onChange }) {
    const input = document.createElement('input');
    input.type = 'number';
    input.min = String(min);
    input.max = String(max);
    input.step = String(step);
    input.value = String(value);
    input.addEventListener('change', () => onChange(input.value));
    return input;
  }

  function canEditLfoDraft() {
    return profileInteractable && !profileWorkflow.isLocked() && !profileWizardBusy && !lfoBusy;
  }

  function markLfoDraftDirty() {
    clearSelectedLfoRoute();
    lfoDraftDirty = true;
    setLfoStatus(
      'busy',
      `${describeSlot()} edited locally. Push & save LFO changes to hear and scope them.`
    );
    updateLfoControls();
  }

  function updateLfoEditorEnabled(canInteract = canEditLfoDraft()) {
    if (!lfoEditor) return;
    lfoEditor.querySelectorAll('input, select, button').forEach((control) => {
      control.disabled = !canInteract;
    });
  }

  function summarizeLfoDraft() {
    const activeRoutes = lfoDraft.routes.length;
    return `${describeSlot()} • ${PROFILE_LFO_COUNT} LFOs • ${activeRoutes} route${
      activeRoutes === 1 ? '' : 's'
    }`;
  }

  function setLfoDraft(nextDraft = {}) {
    const nextLfos = Array.from({ length: PROFILE_LFO_COUNT }, (_, index) => {
      const entries = Array.isArray(nextDraft?.lfos) ? nextDraft.lfos : [];
      const match =
        entries.find?.((entry) => Number(entry?.index) === index) ??
        entries[index] ??
        createDefaultLfoEntry(index);
      return normalizeLfoEntry(match, index);
    });
    const nextRoutes = Array.isArray(nextDraft?.routes)
      ? nextDraft.routes.slice(0, PROFILE_MAX_ROUTES).map((route) => normalizeRouteEntry(route))
      : [];
    lfoDraft = { lfos: nextLfos, routes: nextRoutes };
    clearSelectedLfoRoute();
    lfoDraftDirty = false;
    renderLfoEditor();
    updateLfoControls();
  }

  function updateLfoEntry(index, patch = {}) {
    lfoDraft = {
      ...lfoDraft,
      lfos: lfoDraft.lfos.map((entry, entryIndex) =>
        entryIndex === index ? normalizeLfoEntry({ ...entry, ...patch }, index) : entry
      )
    };
    markLfoDraftDirty();
  }

  function updateRouteEntry(index, patch = {}, { rerender = false } = {}) {
    lfoDraft = {
      ...lfoDraft,
      routes: lfoDraft.routes.map((entry, entryIndex) =>
        entryIndex === index ? normalizeRouteEntry({ ...entry, ...patch }) : entry
      )
    };
    markLfoDraftDirty();
    if (rerender) renderLfoEditor();
  }

  function addLfoRoute() {
    if (lfoDraft.routes.length >= PROFILE_MAX_ROUTES) return;
    const nextRoute =
      lfoDraft.routes.length > 0
        ? normalizeRouteEntry(lfoDraft.routes[lfoDraft.routes.length - 1])
        : normalizeRouteEntry(createDefaultRouteDrafts()[0]);
    lfoDraft = {
      ...lfoDraft,
      routes: [...lfoDraft.routes, nextRoute]
    };
    markLfoDraftDirty();
    renderLfoEditor();
    updateLfoControls();
  }

  function removeLfoRoute(index) {
    lfoDraft = {
      ...lfoDraft,
      routes: lfoDraft.routes.filter((_, routeIndex) => routeIndex !== index)
    };
    markLfoDraftDirty();
    renderLfoEditor();
    updateLfoControls();
  }

  function clearLfoRoutes() {
    lfoDraft = {
      ...lfoDraft,
      routes: []
    };
    markLfoDraftDirty();
    renderLfoEditor();
    updateLfoControls();
  }

  function renderLfoEditor() {
    if (!lfoEditor) return;
    lfoEditor.innerHTML = '';

    const lfoGrid = document.createElement('div');
    lfoGrid.className = 'lfo-grid';
    lfoDraft.lfos.forEach((entry, index) => {
      const section = document.createElement('section');
      section.className = 'lfo-section';
      const heading = document.createElement('h4');
      heading.textContent = `LFO ${index + 1}`;
      section.appendChild(heading);

      const grid = document.createElement('div');
      grid.className = 'lfo-control-grid';
      grid.appendChild(
        createLfoControl(
          'Shape',
          createSelect(LFO_SHAPE_OPTIONS, entry.shape, (value) =>
            updateLfoEntry(index, { shape: value })
          )
        )
      );
      grid.appendChild(
        createLfoControl(
          'Frequency (Hz)',
          createNumberInput({
            value: entry.frequency_hz,
            min: 0.01,
            max: 100,
            step: 0.01,
            onChange: (value) => updateLfoEntry(index, { frequency_hz: value })
          })
        )
      );
      grid.appendChild(
        createLfoControl(
          'Depth',
          createNumberInput({
            value: entry.depth,
            min: 0,
            max: 1,
            step: 0.01,
            onChange: (value) => updateLfoEntry(index, { depth: value })
          })
        )
      );
      grid.appendChild(
        createLfoControl(
          'Bipolar',
          createBooleanSelect(entry.bipolar, (value) => updateLfoEntry(index, { bipolar: value }))
        )
      );
      grid.appendChild(
        createLfoControl(
          'Clock Sync',
          createBooleanSelect(entry.sync, (value) => updateLfoEntry(index, { sync: value }))
        )
      );
      grid.appendChild(
        createLfoControl(
          'Sync Ratio',
          createSelect(LFO_SYNC_RATIO_OPTIONS, entry.sync_ratio, (value) =>
            updateLfoEntry(index, { sync_ratio: value })
          )
        )
      );
      section.appendChild(grid);
      lfoGrid.appendChild(section);
    });
    lfoEditor.appendChild(lfoGrid);

    const routeSection = document.createElement('section');
    routeSection.className = 'lfo-route-section';
    const routeHeading = document.createElement('h4');
    routeHeading.textContent = 'Assignments';
    routeSection.appendChild(routeHeading);

    if (!lfoDraft.routes.length) {
      const empty = document.createElement('p');
      empty.className = 'lfo-empty';
      empty.textContent =
        'No routes saved. Add one to assign an LFO to an internal target, slot, MIDI CC, or OSC.';
      routeSection.appendChild(empty);
    } else {
      const routeList = document.createElement('div');
      routeList.className = 'lfo-route-list';
      lfoDraft.routes.forEach((route, routeIndex) => {
        const card = document.createElement('article');
        card.className = 'lfo-route-card';
        card.dataset.routeIndex = String(routeIndex);
        card.tabIndex = -1;
        if (selectedLfoRouteIndex === routeIndex) {
          card.dataset.selected = 'true';
        }

        const header = document.createElement('header');
        const title = document.createElement('strong');
        title.textContent = `Route ${routeIndex + 1}`;
        const removeBtn = document.createElement('button');
        removeBtn.type = 'button';
        removeBtn.className = 'lfo-route-remove';
        removeBtn.textContent = 'Remove';
        removeBtn.disabled =
          !profileInteractable || profileWorkflow.isLocked() || profileWizardBusy || lfoBusy;
        removeBtn.addEventListener('click', () => removeLfoRoute(routeIndex));
        header.append(title, removeBtn);
        card.appendChild(header);

        const grid = document.createElement('div');
        grid.className = 'lfo-route-grid';
        grid.appendChild(
          createLfoControl(
            'Type',
            createSelect(LFO_ROUTE_TYPE_OPTIONS, route.type, (value) =>
              updateRouteEntry(
                routeIndex,
                {
                  type: value,
                  target: 0,
                  slot: 0,
                  amount: 100,
                  min: 0,
                  max: 127,
                  channel: 1,
                  cc_msb: 0,
                  cc_lsb: 32
                },
                { rerender: true }
              )
            )
          )
        );
        grid.appendChild(
          createLfoControl(
            'Source LFO',
            createSelect(
              Array.from({ length: PROFILE_LFO_COUNT }, (_, index) => `LFO ${index + 1}`),
              route.lfo,
              (value) => updateRouteEntry(routeIndex, { lfo: value })
            )
          )
        );
        grid.appendChild(
          createLfoControl(
            'Depth',
            createNumberInput({
              value: route.depth,
              min: 0,
              max: 1,
              step: 0.01,
              onChange: (value) => updateRouteEntry(routeIndex, { depth: value })
            })
          )
        );
        grid.appendChild(
          createLfoControl(
            'Amount',
            createNumberInput({
              value: route.amount,
              min: -100,
              max: 100,
              step: 1,
              onChange: (value) => updateRouteEntry(routeIndex, { amount: value })
            })
          )
        );
        grid.appendChild(
          createLfoControl(
            'Min',
            createNumberInput({
              value: route.min,
              min: 0,
              max: 127,
              step: 1,
              onChange: (value) => updateRouteEntry(routeIndex, { min: value })
            })
          )
        );
        grid.appendChild(
          createLfoControl(
            'Max',
            createNumberInput({
              value: route.max,
              min: 0,
              max: 127,
              step: 1,
              onChange: (value) => updateRouteEntry(routeIndex, { max: value })
            })
          )
        );
        if (route.type === 0) {
          grid.appendChild(
            createLfoControl(
              'Target',
              createSelect(LFO_INTERNAL_TARGET_OPTIONS, route.target, (value) =>
                updateRouteEntry(routeIndex, { target: value })
              )
            )
          );
        }
        if (route.type === 1 || route.type === 2) {
          grid.appendChild(
            createLfoControl(
              'MIDI Channel',
              createNumberInput({
                value: route.channel,
                min: 1,
                max: 16,
                step: 1,
                onChange: (value) => updateRouteEntry(routeIndex, { channel: value })
              })
            )
          );
          grid.appendChild(
            createLfoControl(
              'CC MSB',
              createNumberInput({
                value: route.cc_msb,
                min: 0,
                max: 127,
                step: 1,
                onChange: (value) => updateRouteEntry(routeIndex, { cc_msb: value })
              })
            )
          );
        }
        if (route.type === 2) {
          grid.appendChild(
            createLfoControl(
              'CC LSB',
              createNumberInput({
                value: route.cc_lsb,
                min: 0,
                max: 127,
                step: 1,
                onChange: (value) => updateRouteEntry(routeIndex, { cc_lsb: value })
              })
            )
          );
        }
        if (route.type === 4) {
          grid.appendChild(
            createLfoControl(
              'Slot',
              createNumberInput({
                value: route.slot + 1,
                min: 1,
                max: SLOT_COUNT,
                step: 1,
                onChange: (value) => {
                  const slotIndex = clampInteger(value, 1, SLOT_COUNT, 1) - 1;
                  updateRouteEntry(routeIndex, { slot: slotIndex, target: slotIndex });
                }
              })
            )
          );
        }
        card.appendChild(grid);
        routeList.appendChild(card);
      });
      routeSection.appendChild(routeList);
    }

    lfoEditor.appendChild(routeSection);
    updateLfoEditorEnabled();
  }

  // Synchronize the macro/scene/profile helper text with the current capability set.
  function syncRecoverySupportCopy() {
    updateProfileHint();
    if (!profileInteractable) {
      setMacroStatus('muted', 'Connect to see whether macro storage is available.');
      sceneControls.syncSupportCopy();
      return;
    }
    if (!deviceCapabilities.macroSnapshot) {
      setMacroStatus('muted', 'Macro snapshot storage is unavailable on this firmware.');
    } else if (!macroBusy && !macroAvailable) {
      setMacroStatus('muted', 'No macro snapshot stored yet.');
    }
    sceneControls.syncSupportCopy();
  }

  // Translate a slot index into the A-D label shown in the profile controls.
  function slotLabel(index) {
    return PROFILE_LABELS[index] ?? PROFILE_LABELS[0];
  }

  // User-facing description for the currently targeted profile slot.
  function describeSlot(index = activeProfileSlot) {
    return `Slot ${slotLabel(index)}`;
  }

  function describeBoardActiveSlot() {
    return describeSlot(deviceActiveProfileSlot);
  }

  function readActiveProfileIndex(payload = {}) {
    const candidate = Number(payload?.active_profile ?? payload?.activeProfile);
    if (!Number.isFinite(candidate)) return null;
    return clampProfileSlot(candidate, PROFILE_LABELS.length);
  }

  function syncDeviceActiveProfile(payload = {}, { refresh = false, alignTarget = false } = {}) {
    const nextActive = readActiveProfileIndex(payload);
    if (nextActive === null) return false;
    const changed = nextActive !== deviceActiveProfileSlot;
    deviceActiveProfileSlot = nextActive;
    if (alignTarget && nextActive !== activeProfileSlot) {
      setActiveProfileSlot(nextActive, { persist: false });
    } else if (changed && profileSlotStatus) {
      profileSlotStatus.textContent = `${describeSlot(
        activeProfileSlot
      )} • ${profileSlotModeCopy()}`;
    }
    if (
      refresh &&
      changed &&
      activeProfileSlot === nextActive &&
      profileInteractable &&
      !lfoDraftDirty &&
      !arpBusy &&
      !lfoBusy
    ) {
      void refreshProfileUtilities({ silent: true });
    }
    return changed;
  }

  // Update the selected profile slot and mirror it into the UI/state badges.
  function setActiveProfileSlot(index, { persist = true } = {}) {
    const bounded = clampProfileSlot(index, PROFILE_LABELS.length);
    activeProfileSlot = bounded;
    profileSlotButtons.forEach((button) => {
      const slotValue = Number(button.dataset.profileSlot);
      if (!Number.isFinite(slotValue)) return;
      button.setAttribute('aria-pressed', slotValue === bounded ? 'true' : 'false');
    });
    if (profileSlotStatus) {
      profileSlotStatus.textContent = `${describeSlot(bounded)} • ${profileSlotModeCopy()}`;
    }
    if (persist) {
      persistProfileSlot(bounded, { slotCount: PROFILE_LABELS.length });
    }
  }

  // Recompute enabled/disabled state for all profile/macro/scene controls.
  function refreshProfileControls() {
    const canInteract = profileInteractable && !profileWorkflow.isLocked();
    if (profileSaveBtn) profileSaveBtn.disabled = !canInteract || !deviceCapabilities.profileSave;
    if (profileLoadBtn) profileLoadBtn.disabled = !canInteract || !deviceCapabilities.profileLoad;
    if (profileResetBtn)
      profileResetBtn.disabled = !canInteract || !deviceCapabilities.profileReset;
    if (applySaveProfileBtn) {
      applySaveProfileBtn.disabled =
        !canInteract || !deviceCapabilities.profileSave || !runtime.getState().dirty;
      applySaveProfileBtn.title = deviceCapabilities.profileSave
        ? 'Apply staged edits, then save the active device profile slot.'
        : 'This firmware does not expose browser-driven profile save.';
    }
    updateProfileWizardControls();
    updateMacroControls();
    updateArpControls();
    updateLiveArpControls();
    updateLfoControls();
    updateModMatrixControls();
    sceneControls.updateControls();
    updateProfileHint();
  }

  function updateArpControls() {
    const canInteract =
      profileInteractable && !profileWorkflow.isLocked() && !profileWizardBusy && !arpBusy;
    if (arpRefreshBtn) arpRefreshBtn.disabled = !canInteract;
    if (arpSaveBtn) arpSaveBtn.disabled = !canInteract;
    [arpLengthInput, arpShapeSelect, arpSwingInput, arpGateInput, arpOctaveInput].forEach(
      (control) => {
        if (control) control.disabled = !canInteract;
      }
    );
    if (arpCard) {
      arpCard.dataset.state = canInteract ? 'ready' : 'muted';
    }
  }

  function updateLiveArpControls() {
    const canInteract =
      profileInteractable &&
      !profileWorkflow.isLocked() &&
      !profileWizardBusy &&
      !liveArpBusy &&
      deviceCapabilities.arpLive;
    [
      liveArpSlotInput,
      liveArpLengthInput,
      liveArpShapeSelect,
      liveArpSwingInput,
      liveArpGateInput,
      liveArpOctaveInput
    ].forEach((control) => {
      if (control) control.disabled = !canInteract;
    });
    if (liveArpRefreshBtn) liveArpRefreshBtn.disabled = !canInteract;
    if (liveArpApplyBtn) liveArpApplyBtn.disabled = !canInteract;
    if (liveArpStartBtn) {
      liveArpStartBtn.disabled = !canInteract;
      liveArpStartBtn.setAttribute('aria-pressed', liveArpDraft.active ? 'true' : 'false');
      liveArpStartBtn.textContent = liveArpDraft.active
        ? `Running slot ${liveArpDraft.slot}`
        : `Start slot ${liveArpDraft.slot}`;
    }
    if (liveArpStopBtn) liveArpStopBtn.disabled = !canInteract || !liveArpDraft.active;
  }

  function updateLfoControls() {
    const canInteract = canEditLfoDraft();
    if (lfoRefreshBtn) lfoRefreshBtn.disabled = !canInteract;
    if (lfoSaveBtn) {
      lfoSaveBtn.disabled = !canInteract || !lfoDraftDirty;
      lfoSaveBtn.textContent = lfoDraftDirty ? 'Push & save LFO changes' : 'Save slot LFOs';
      lfoSaveBtn.title = lfoDraftDirty
        ? `Apply local LFO edits live and persist them to ${describeSlot()}.`
        : `No unsaved LFO edits for ${describeSlot()}.`;
    }
    if (lfoRouteAddBtn) {
      lfoRouteAddBtn.disabled = !canInteract || lfoDraft.routes.length >= PROFILE_MAX_ROUTES;
    }
    if (lfoRoutesClearBtn) lfoRoutesClearBtn.disabled = !canInteract || !lfoDraft.routes.length;
    if (lfoCard) {
      lfoCard.dataset.state = canInteract ? 'ready' : 'muted';
    }
    updateLfoEditorEnabled(canInteract);
  }

  function updateModMatrixControls() {
    const canInteract = profileInteractable && !profileWorkflow.isLocked() && !modMatrixBusy;
    if (modMatrixRefreshBtn) modMatrixRefreshBtn.disabled = !canInteract;
  }

  // Single sink for the guided profile wizard status line.
  function setProfileWizardStatus(state, message) {
    if (!profileWizardStatus) return;
    profileWizardStatus.dataset.state = state;
    profileWizardStatus.textContent = message;
  }

  // Drive the three-step profile wizard copy and button gating from current state.
  function updateProfileWizardControls() {
    // Guided flow state machine: switch to target slot -> apply dirty edits -> save slot.
    const target = clampProfileSlot(
      Number(profileWizardTarget?.value ?? profileWizardTargetSlot),
      PROFILE_LABELS.length
    );
    profileWizardTargetSlot = target;
    const guidedSupported = supportsGuidedProfileFlow();
    const canInteract =
      profileInteractable && !profileWorkflow.isLocked() && !profileWizardBusy && guidedSupported;
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
    if (!guidedSupported) {
      setProfileWizardStatus(
        'muted',
        'This firmware does not expose browser-driven profile switch/save yet. Use Download/Upload for file backups.'
      );
      return;
    }
    if (!onTarget) {
      setProfileWizardStatus(
        'busy',
        `Step 1: switch to Slot ${slotLabel(target)} (currently ${slotLabel(activeProfileSlot)}).`
      );
      return;
    }
    if (dirtyNow) {
      setProfileWizardStatus('busy', `Step 2: apply edits, then save Slot ${slotLabel(target)}.`);
      return;
    }
    setProfileWizardStatus(
      'ok',
      `Step 3: save Slot ${slotLabel(target)} to store current mapping.`
    );
  }

  // Gate macro buttons on capability, connectivity, and pending RPC state.
  function updateMacroControls() {
    const offline = !profileInteractable;
    const unsupported = !deviceCapabilities.macroSnapshot;
    if (macroSaveBtn) macroSaveBtn.disabled = offline || macroBusy || unsupported;
    if (macroRecallBtn) {
      macroRecallBtn.disabled = offline || macroBusy || unsupported || !macroAvailable;
    }
  }

  // Update the macro status copy line.
  function setMacroStatus(state, message) {
    if (!macroStatusEl) return;
    macroStatusEl.dataset.state = state;
    if (typeof message === 'string') {
      macroStatusEl.textContent = message;
    }
  }

  async function runMacroCommand(command) {
    if (!deviceCapabilities.macroSnapshot) {
      setMacroStatus('muted', 'Macro snapshot storage is unavailable on this firmware.');
      setStatus(
        'warn',
        'Macro unavailable',
        'This firmware does not expose macro snapshot storage to the browser.'
      );
      return;
    }
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
        isSave
          ? 'Macro snapshot stored in EEPROM slot 254.'
          : 'Macro snapshot recalled from EEPROM slot 254.'
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

  function applyProfileUtilityDrafts(response = {}) {
    setArpDraft(response?.arp ?? createDefaultArpDraft());
    setLfoDraft({
      lfos: Array.isArray(response?.lfos) ? response.lfos : createDefaultLfoDraft().lfos,
      routes: Array.isArray(response?.routes) ? response.routes : createDefaultLfoDraft().routes
    });
  }

  async function refreshProfileUtilities({ silent = false, focus = 'all' } = {}) {
    if (!profileInteractable) {
      setArpStatus('muted', 'Connect to inspect the selected profile slot.');
      setLfoStatus('muted', 'Connect to inspect the selected profile slot.');
      return;
    }
    if (profileWorkflow.isLocked() || arpBusy || lfoBusy) return;
    arpBusy = true;
    lfoBusy = true;
    refreshProfileControls();
    if (!silent) {
      if (focus === 'arp') {
        setArpStatus('busy', `Loading arp settings from ${describeSlot()}…`);
      } else if (focus === 'lfo') {
        setLfoStatus('busy', `Loading LFO settings from ${describeSlot()}…`);
      } else {
        setArpStatus('busy', `Loading profile modulation from ${describeSlot()}…`);
        setLfoStatus('busy', `Loading profile modulation from ${describeSlot()}…`);
      }
    }
    try {
      const response = await runtime.sendRpc(
        { rpc: 'get_profile', slot: activeProfileSlot, scope: 'modulation' },
        { timeoutMs: PROFILE_RPC_TIMEOUT_MS, rollbackOnError: false }
      );
      syncDeviceActiveProfile(response);
      applyProfileUtilityDrafts(response);
      const shapeLabel = ARP_SHAPE_OPTIONS[arpDraft.shape] ?? `Shape ${arpDraft.shape}`;
      setArpStatus('ok', `${describeSlot()} • ${shapeLabel} • ${arpDraft.length_ticks} ticks`);
      setLfoStatus('ok', summarizeLfoDraft());
    } catch (err) {
      const message = err.message || String(err);
      if (focus === 'lfo') {
        setLfoStatus('err', `LFO read failed: ${message}`);
      } else if (focus === 'arp') {
        setArpStatus('err', `Arp read failed: ${message}`);
      } else {
        setArpStatus('err', `Profile modulation read failed: ${message}`);
        setLfoStatus('err', `Profile modulation read failed: ${message}`);
      }
    } finally {
      arpBusy = false;
      lfoBusy = false;
      refreshProfileControls();
      renderLfoEditor();
    }
  }

  async function saveArpProfile() {
    if (!profileInteractable || profileWorkflow.isLocked() || arpBusy) return;
    readArpFormIntoDraft();
    arpBusy = true;
    refreshProfileControls();
    setArpStatus('busy', `Saving arp settings to ${describeSlot()}…`);
    try {
      await runtime.sendRpc(
        { rpc: 'set_profile', slot: activeProfileSlot, profile: { arp: arpDraft } },
        { timeoutMs: PROFILE_RPC_TIMEOUT_MS }
      );
      setArpStatus(
        'ok',
        `${describeSlot()} saved. If this slot is active, the runtime updates now.`
      );
      setStatus('ok', 'Arp profile saved', describeSlot());
    } catch (err) {
      setArpStatus('err', `Arp save failed: ${err.message || String(err)}`);
      setStatus('err', 'Arp profile save failed', err.message || String(err));
    } finally {
      arpBusy = false;
      refreshProfileControls();
    }
  }

  function summarizeLiveArp() {
    const shapeLabel = ARP_SHAPE_OPTIONS[liveArpDraft.shape] ?? `Shape ${liveArpDraft.shape}`;
    const active = liveArpDraft.active ? `Slot ${liveArpDraft.slot}` : 'Idle';
    return `${active} • ${shapeLabel} • ${liveArpDraft.length_ticks} ticks`;
  }

  async function refreshLiveArp({ silent = false } = {}) {
    if (!profileInteractable || !deviceCapabilities.arpLive || liveArpBusy) return;
    liveArpBusy = true;
    refreshProfileControls();
    if (!silent) setLiveArpStatus('busy', 'Reading live arp state…');
    try {
      const response = await runtime.sendRpc(
        { rpc: 'get_arp' },
        { timeoutMs: PROFILE_RPC_TIMEOUT_MS, rollbackOnError: false }
      );
      setLiveArpDraft({
        ...response,
        slot: response?.active
          ? response.slot
          : liveArpSlotFollowsSelection
            ? getSelectedSlot()
            : liveArpDraft.slot
      });
      setLiveArpStatus('ok', summarizeLiveArp());
    } catch (err) {
      setLiveArpStatus('err', `Live arp read failed: ${err.message || String(err)}`);
    } finally {
      liveArpBusy = false;
      refreshProfileControls();
    }
  }

  async function pushLiveArp() {
    if (!profileInteractable || !deviceCapabilities.arpLive || liveArpBusy) return;
    readLiveArpFormIntoDraft();
    liveArpBusy = true;
    refreshProfileControls();
    setLiveArpStatus('busy', 'Pushing live arp controls…');
    try {
      const response = await runtime.sendRpc(
        {
          rpc: 'set_arp',
          lengthTicks: liveArpDraft.length_ticks,
          shape: liveArpDraft.shape,
          swingPercent: liveArpDraft.swing_percent,
          gatePercent: liveArpDraft.gate_percent,
          octaveRange: liveArpDraft.octave_range
        },
        { timeoutMs: PROFILE_RPC_TIMEOUT_MS, rollbackOnError: false }
      );
      setLiveArpDraft({ ...response, slot: liveArpDraft.slot });
      setLiveArpStatus('ok', summarizeLiveArp());
      setStatus('ok', 'Live arp updated', summarizeLiveArp());
    } catch (err) {
      setLiveArpStatus('err', `Live arp update failed: ${err.message || String(err)}`);
      setStatus('err', 'Live arp update failed', err.message || String(err));
    } finally {
      liveArpBusy = false;
      refreshProfileControls();
    }
  }

  async function startLiveArp() {
    if (!profileInteractable || !deviceCapabilities.arpLive || liveArpBusy) return;
    readLiveArpFormIntoDraft();
    liveArpBusy = true;
    refreshProfileControls();
    setLiveArpStatus('busy', `Starting arp on slot ${liveArpDraft.slot}…`);
    try {
      await runtime.sendRpc(
        { rpc: 'arp_start', slot: liveArpDraft.slot },
        { timeoutMs: PROFILE_RPC_TIMEOUT_MS, rollbackOnError: false }
      );
      setLiveArpDraft({ ...liveArpDraft, active: true });
      setLiveArpStatus('ok', summarizeLiveArp());
    } catch (err) {
      setLiveArpStatus('err', `Arp start failed: ${err.message || String(err)}`);
      setStatus('err', 'Arp start failed', err.message || String(err));
    } finally {
      liveArpBusy = false;
      refreshProfileControls();
    }
  }

  async function stopLiveArp() {
    if (!profileInteractable || !deviceCapabilities.arpLive || liveArpBusy) return;
    liveArpBusy = true;
    refreshProfileControls();
    setLiveArpStatus('busy', 'Stopping arp…');
    try {
      await runtime.sendRpc(
        { rpc: 'arp_stop' },
        { timeoutMs: PROFILE_RPC_TIMEOUT_MS, rollbackOnError: false }
      );
      setLiveArpDraft({
        ...liveArpDraft,
        active: false,
        slot: liveArpSlotFollowsSelection ? getSelectedSlot() : liveArpDraft.slot
      });
      setLiveArpStatus('ok', summarizeLiveArp());
    } catch (err) {
      setLiveArpStatus('err', `Arp stop failed: ${err.message || String(err)}`);
      setStatus('err', 'Arp stop failed', err.message || String(err));
    } finally {
      liveArpBusy = false;
      refreshProfileControls();
    }
  }

  async function saveLfoProfile() {
    if (!profileInteractable || profileWorkflow.isLocked() || lfoBusy) return;
    lfoBusy = true;
    const targetSlot = activeProfileSlot;
    refreshProfileControls();
    setLfoStatus('busy', `Saving LFO settings to ${describeSlot(targetSlot)}…`);
    try {
      const response = await runtime.sendRpc(
        {
          rpc: 'set_profile',
          slot: targetSlot,
          profile: {
            lfos: lfoDraft.lfos.map((entry, index) => ({
              index,
              shape: entry.shape,
              frequency_hz: entry.frequency_hz,
              depth: entry.depth,
              bipolar: entry.bipolar,
              sync: entry.sync,
              sync_ratio: entry.sync_ratio
            })),
            routes: lfoDraft.routes.map((route) => ({
              type: route.type,
              lfo: route.lfo,
              depth: route.depth,
              amount: route.amount,
              min: route.min,
              max: route.max,
              target: route.type === 4 ? route.slot : route.target,
              ...(route.type === 4 ? { slot: route.slot } : {}),
              channel: route.channel,
              cc_msb: route.cc_msb,
              cc_lsb: route.cc_lsb
            }))
          }
        },
        { timeoutMs: PROFILE_RPC_TIMEOUT_MS }
      );
      const responseActiveSlot = readActiveProfileIndex(response);
      syncDeviceActiveProfile(response);
      const targetIsBoardActive =
        responseActiveSlot === targetSlot || deviceActiveProfileSlot === targetSlot;
      const liveApplyAckMissing = response?.active_applied === false && targetIsBoardActive;
      const liveApplied = response?.active_applied !== false || targetIsBoardActive;
      setLfoStatus(
        liveApplied ? 'ok' : 'busy',
        liveApplied && !liveApplyAckMissing
          ? `${describeSlot(targetSlot)} saved and applied live with ${
              lfoDraft.routes.length
            } routes.`
          : liveApplyAckMissing
            ? `${describeSlot(
                targetSlot
              )} saved to the active board profile. Reopen or refresh the scope if the LFOs do not move.`
            : `${describeSlot(
                targetSlot
              )} saved, but ${describeBoardActiveSlot()} is active on the board. Switch to ${describeSlot(
                targetSlot
              )} to hear and scope these LFOs.`
      );
      lfoDraftDirty = false;
      setStatus(
        liveApplied ? 'ok' : 'warn',
        liveApplied
          ? liveApplyAckMissing
            ? 'LFO profile saved to active slot'
            : 'LFO profile saved'
          : 'LFO profile saved, not active',
        liveApplied
          ? describeSlot(targetSlot)
          : `${describeSlot(targetSlot)} saved • board active ${slotLabel(deviceActiveProfileSlot)}`
      );
    } catch (err) {
      setLfoStatus('err', `LFO save failed: ${err.message || String(err)}`);
      setStatus('err', 'LFO profile save failed', err.message || String(err));
    } finally {
      lfoBusy = false;
      refreshProfileControls();
      renderLfoEditor();
    }
  }

  async function refreshModMatrix() {
    if (!profileInteractable || profileWorkflow.isLocked() || modMatrixBusy) return;
    modMatrixBusy = true;
    updateModMatrixControls();
    setModMatrixStatus('busy', 'Reading modulation matrix…');
    try {
      modMatrixReport = await runtime.sendRpc(
        { rpc: 'get_mod_matrix' },
        { timeoutMs: PROFILE_RPC_TIMEOUT_MS, rollbackOnError: false }
      );
      renderModMatrix(modMatrixReport);
      const conflictCount = Array.isArray(modMatrixReport?.conflicts)
        ? modMatrixReport.conflicts.length
        : 0;
      setModMatrixStatus(
        conflictCount ? 'busy' : 'ok',
        conflictCount
          ? `${conflictCount} modulation collision${conflictCount === 1 ? '' : 's'} reported.`
          : 'Modulation matrix loaded.'
      );
    } catch (err) {
      setModMatrixStatus('err', `Matrix read failed: ${err.message || String(err)}`);
    } finally {
      modMatrixBusy = false;
      updateModMatrixControls();
    }
  }

  async function handleWizardSwitchProfile() {
    // Step 1: bind the UI to the selected slot and pull that slot's config into staged/live state.
    if (!profileInteractable || profileWorkflow.isLocked() || profileWizardBusy) return;
    profileWizardBusy = true;
    refreshProfileControls();
    try {
      setActiveProfileSlot(profileWizardTargetSlot);
      await runProfileRpc('load_profile', {
        busyLabel: 'Switching profile…',
        successLabel: 'Profile switched',
        successCopy: `${describeSlot()} active`,
        expectConfig: true
      });
    } finally {
      profileWizardBusy = false;
      refreshProfileControls();
    }
  }

  async function handleWizardApplyEdits() {
    // Step 2: commit staged edits to firmware (without persisting profile slot yet).
    if (!profileInteractable || profileWorkflow.isLocked() || profileWizardBusy) return;
    if (activeProfileSlot !== profileWizardTargetSlot) {
      setStatus(
        'warn',
        'Wrong slot',
        `Switch to Slot ${slotLabel(profileWizardTargetSlot)} first.`
      );
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
    if (!profileInteractable || profileWorkflow.isLocked() || profileWizardBusy) return;
    if (activeProfileSlot !== profileWizardTargetSlot) {
      setStatus(
        'warn',
        'Wrong slot',
        `Switch to Slot ${slotLabel(profileWizardTargetSlot)} first.`
      );
      return;
    }
    profileWizardBusy = true;
    refreshProfileControls();
    try {
      await runProfileRpc('save_profile', {
        busyLabel: 'Saving profile…',
        successLabel: 'Profile saved',
        successCopy: `${describeSlot()} archived`
      });
    } finally {
      profileWizardBusy = false;
      refreshProfileControls();
    }
  }

  function bind() {
    if (bound) return;
    bound = true;
    sceneControls.bind();

    profileSlotButtons.forEach((button) => {
      button.addEventListener('click', () => {
        const slotIndex = clampProfileSlot(
          Number(button.dataset.profileSlot),
          PROFILE_LABELS.length
        );
        setActiveProfileSlot(slotIndex);
        void refreshProfileUtilities({ silent: true });
      });
    });
    if (profileWizardTarget) {
      profileWizardTarget.value = String(profileWizardTargetSlot);
      profileWizardTarget.addEventListener('change', () => {
        profileWizardTargetSlot = clampProfileSlot(
          Number(profileWizardTarget.value),
          PROFILE_LABELS.length
        );
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
    setArpDraft(createDefaultArpDraft());
    setLfoDraft(createDefaultLfoDraft());
    setArpStatus('muted', 'Connect to inspect the selected profile slot.');
    setLiveArpDraft(liveArpDraft);
    setLiveArpStatus('muted', 'Connect to inspect live arpeggiator state.');
    setLfoStatus('muted', 'Connect to inspect the selected profile slot.');
    [arpLengthInput, arpShapeSelect, arpSwingInput, arpGateInput, arpOctaveInput].forEach(
      (control) => control?.addEventListener('change', () => readArpFormIntoDraft())
    );
    [
      liveArpSlotInput,
      liveArpLengthInput,
      liveArpShapeSelect,
      liveArpSwingInput,
      liveArpGateInput,
      liveArpOctaveInput
    ].forEach((control) => control?.addEventListener('change', () => readLiveArpFormIntoDraft()));
    arpRefreshBtn?.addEventListener('click', () => refreshProfileUtilities({ focus: 'arp' }));
    arpSaveBtn?.addEventListener('click', () => saveArpProfile());
    liveArpRefreshBtn?.addEventListener('click', () => refreshLiveArp());
    liveArpApplyBtn?.addEventListener('click', () => pushLiveArp());
    liveArpStartBtn?.addEventListener('click', () => startLiveArp());
    liveArpStopBtn?.addEventListener('click', () => stopLiveArp());
    lfoRouteAddBtn?.addEventListener('click', () => addLfoRoute());
    lfoRoutesClearBtn?.addEventListener('click', () => clearLfoRoutes());
    lfoRefreshBtn?.addEventListener('click', () => refreshProfileUtilities({ focus: 'lfo' }));
    lfoSaveBtn?.addEventListener('click', () => saveLfoProfile());
    modMatrixRefreshBtn?.addEventListener('click', () => refreshModMatrix());
    renderModMatrix();
    setModMatrixStatus('muted', 'Connect to inspect modulation routes.');

    profileSaveBtn?.addEventListener('click', () =>
      runProfileRpc('save_profile', {
        busyLabel: 'Saving profile…',
        successLabel: 'Profile saved',
        successCopy: `${describeSlot()} archived`
      })
    );
    profileLoadBtn?.addEventListener('click', () =>
      runProfileRpc('load_profile', {
        busyLabel: 'Switching profile…',
        successLabel: 'Profile switched',
        successCopy: `${describeSlot()} active`,
        expectConfig: true
      })
    );
    profileResetBtn?.addEventListener('click', () =>
      runProfileRpc('reset_profile', {
        busyLabel: 'Resetting profile…',
        successLabel: 'Profile reset',
        successCopy: `${describeSlot()} restored`,
        expectConfig: true
      })
    );
    profileDownloadBtn?.addEventListener('click', () => profileFileIO.handleProfileDownload());
    profileUploadBtn?.addEventListener('click', () => profileFileIO.handleProfileUpload());
    applySaveProfileBtn?.addEventListener('click', () =>
      runProfileRpc('save_profile', {
        busyLabel: 'Saving profile…',
        successLabel: 'Profile saved',
        successCopy: `${describeSlot()} applied and archived`
      })
    );
  }

  function onConfigChanged() {
    refreshProfileControls();
    updateProfileWizardControls();
  }

  function onManifest(manifest) {
    deviceCapabilities = resolveProfileCapabilities(manifest);
    macroAvailable = deviceCapabilities.macroSnapshot ? macroAvailable : false;
    syncDeviceActiveProfile(manifest, { alignTarget: true });
    setActiveProfileSlot(activeProfileSlot, { persist: false });
    syncRecoverySupportCopy();
    refreshProfileControls();
  }

  function onTelemetry(frame = {}) {
    if (lfoDraftDirty || profileWorkflow.isLocked() || arpBusy || lfoBusy) return;
    syncDeviceActiveProfile(frame, { refresh: true });
  }

  function onConnected() {
    profileInteractable = true;
    setLiveArpSlot(getSelectedSlot());
    refreshProfileControls();
    renderLfoEditor();
    syncRecoverySupportCopy();
    void refreshProfileUtilities({ silent: true });
    if (deviceCapabilities.arpLive) {
      void refreshLiveArp({ silent: true });
    } else {
      setLiveArpStatus('muted', 'This firmware does not expose live arp controls.');
    }
    if (deviceCapabilities.scenes) {
      sceneControls.refreshSceneList();
    }
  }

  function onDisconnected() {
    deviceCapabilities = resolveProfileCapabilities(localManifest);
    profileInteractable = false;
    profileWorkflow.reset();
    macroAvailable = false;
    arpBusy = false;
    lfoBusy = false;
    modMatrixBusy = false;
    modMatrixReport = null;
    liveArpBusy = false;
    setActiveProfileSlot(activeProfileSlot, { persist: false });
    refreshProfileControls();
    renderLfoEditor();
    renderModMatrix();
    syncRecoverySupportCopy();
    setArpStatus('muted', 'Connect to inspect the selected profile slot.');
    setLiveArpStatus('muted', 'Connect to inspect live arpeggiator state.');
    setLfoStatus('muted', 'Connect to inspect the selected profile slot.');
    setModMatrixStatus('muted', 'Connect to inspect modulation routes.');
  }

  function onRuntimeError() {
    deviceCapabilities = resolveProfileCapabilities(localManifest);
    profileInteractable = false;
    profileWorkflow.reset();
    macroAvailable = false;
    arpBusy = false;
    lfoBusy = false;
    modMatrixBusy = false;
    modMatrixReport = null;
    liveArpBusy = false;
    setActiveProfileSlot(activeProfileSlot, { persist: false });
    refreshProfileControls();
    renderLfoEditor();
    renderModMatrix();
    syncRecoverySupportCopy();
    setArpStatus('muted', 'Reconnect to inspect or save arp settings.');
    setLiveArpStatus('muted', 'Reconnect to inspect live arpeggiator state.');
    setLfoStatus('muted', 'Reconnect to inspect or save LFO settings.');
    setModMatrixStatus('muted', 'Reconnect to inspect modulation routes.');
  }

  function onMacro({ available } = {}) {
    if (!deviceCapabilities.macroSnapshot) return;
    if (available === undefined) return;
    macroAvailable = Boolean(available);
    if (!macroAvailable && !macroBusy) {
      setMacroStatus('muted', 'No macro snapshot stored yet.');
    }
    updateMacroControls();
  }

  return {
    bind,
    setLiveArpSlot,
    onConfigChanged,
    onManifest,
    onTelemetry,
    onConnected,
    onDisconnected,
    onRuntimeError,
    onMacro,
    onScene: sceneControls.onScene
  };
}
