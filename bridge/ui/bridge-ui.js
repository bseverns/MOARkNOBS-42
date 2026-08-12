/* eslint-env browser */

const STORAGE_KEY = 'mn42-bridge-console';
const MODE_KEY = 'mn42-bridge-console-mode';
const CUSTOM_SETUPS_KEY = 'mn42-bridge-custom-host-setups-v1';
const RAW_LINE_LIMIT = 200;
const STRUCTURED_EVENT_LIMIT = 120;
const controlToken = new URL(window.location.href).searchParams.get('token') || '';
const {
  activeAlerts: selectActiveAlerts,
  describeAuthority,
  describeConfigValidation,
  describeDraft,
  describeRouteHeartbeats,
  describeRoutingDestinations,
  createHostSetupEnvelope,
  formatTelemetryFreshness,
  hostSetupConfigFingerprint,
  isActionVisibleInMode,
  latestLearnableMidiCc,
  normalizeHostSetupConfig,
  observedSoundcheckLanes,
  operatorConfirmationMessage,
  parseHostSetupEnvelope,
  parseSlotTelemetryLine,
  recentOscAddresses,
  changedSlotIndices,
} = window.MN42BridgeOperatorState;

const form = document.getElementById('bridge-form');
const startButton = document.getElementById('start-bridge');
const stopButton = document.getElementById('stop-bridge');
const refreshPortsButton = document.getElementById('refresh-ports');
const resetMetricsButton = document.getElementById('reset-metrics');
const clearAlertsButton = document.getElementById('clear-alerts');
const downloadSnapshotButton = document.getElementById('download-snapshot');
const openConfiguratorButton = document.getElementById('open-configurator');
const summaryStatus = document.getElementById('summary-status');
const portsList = document.getElementById('ports');
const portsDatalist = document.getElementById('serial-port-list');
const midiInputsList = document.getElementById('midi-inputs');
const midiOutputsList = document.getElementById('midi-outputs');
const midiPortsDatalist = document.getElementById('midi-port-list');
const logOutput = document.getElementById('logs');
const alertHistory = document.getElementById('alert-history');
const presetSelect = document.getElementById('preset-select');
const recipeSummary = document.getElementById('recipe-summary');
const recipeRequirements = document.getElementById('recipe-requirements');
const recipeChecklist = document.getElementById('recipe-checklist');
const routeOutput = document.getElementById('route-output');
const rawSerialOutput = document.getElementById('raw-serial-output');
const stateJsonOutput = document.getElementById('state-json-output');
const mappingOutput = document.getElementById('mapping-output');
const stageOpenConfiguratorButton = document.getElementById(
  'stage-open-configurator',
);
const stageDownloadSnapshotButton = document.getElementById(
  'stage-download-snapshot',
);
const stageRefreshStateButton = document.getElementById('stage-refresh-state');
const stageRecoveryNeeded = document.getElementById('stage-recovery-needed');
const addMappingForm = document.getElementById('add-mapping-form');
const mappingListBody = document.getElementById('mapping-list-body');
const startSoundcheckButton = document.getElementById('start-soundcheck');
const soundcheckStatus = document.getElementById('soundcheck-status');
const startMappingLearnButton = document.getElementById('start-mapping-learn');
const mappingLearnStatus = document.getElementById('mapping-learn-status');
const recentOscAddressSelect = document.getElementById('recent-osc-address');
const mappingPreview = document.getElementById('mapping-preview');
const customSetupSelect = document.getElementById('custom-setup-select');
const customSetupName = document.getElementById('custom-setup-name');
const customSetupProfile = document.getElementById('custom-setup-profile');
const customSetupNotes = document.getElementById('custom-setup-notes');
const customSetupStatus = document.getElementById('custom-setup-status');
const stagePerformanceSetup = document.getElementById('stage-performance-setup');
const newCustomSetupButton = document.getElementById('new-custom-setup');
const saveCustomSetupButton = document.getElementById('save-custom-setup');
const loadCustomSetupButton = document.getElementById('load-custom-setup');
const deleteCustomSetupButton = document.getElementById('delete-custom-setup');
const exportCustomSetupsButton = document.getElementById(
  'export-custom-setups',
);
const importCustomSetupsInput = document.getElementById(
  'import-custom-setups',
);
const outboundMidiMappingsInput = document.getElementById('outbound-midi-mappings');
const outboundMidiMappingStatus = document.getElementById('outbound-midi-mapping-status');
const midiTelemetryModeSelect = document.getElementById('midi-telemetry-mode');

const routeHeartbeatNodes = {
  deviceOsc: document.getElementById('route-device-osc'),
  deviceMidi: document.getElementById('route-device-midi'),
  oscDevice: document.getElementById('route-osc-device'),
  midiDevice: document.getElementById('route-midi-device'),
};

const modeTabs = [...document.querySelectorAll('.mode-tab')];
const modeViews = [...document.querySelectorAll('[data-mode-view]')];
const modeScopedActions = [...document.querySelectorAll('[data-console-modes]')];

const statusNodes = {
  running: document.getElementById('status-running'),
  serial: document.getElementById('status-serial'),
  ready: document.getElementById('status-ready'),
  osc: document.getElementById('status-osc'),
  oscListen: document.getElementById('status-osc-listen'),
  midi: document.getElementById('status-midi'),
  telemetry: document.getElementById('status-telemetry'),
  lastRoute: document.getElementById('status-last-route'),
  lastTrace: document.getElementById('status-last-trace'),
  sourceTs: document.getElementById('status-source-ts'),
  sourceSkew: document.getElementById('status-source-skew'),
  roundTripSamples: document.getElementById('status-rt-samples'),
  roundTripPending: document.getElementById('status-rt-pending'),
  roundTripLast: document.getElementById('status-rt-last'),
  roundTripP50: document.getElementById('status-rt-p50'),
  roundTripP95: document.getElementById('status-rt-p95'),
  roundTripJitterP95: document.getElementById('status-rt-jitter-p95'),
  roundTripHealth: document.getElementById('status-rt-health'),
  alertCount: document.getElementById('status-alert-count'),
  alertTop: document.getElementById('status-alert-top'),
  error: document.getElementById('status-error'),
  serialParseDrops: document.getElementById('status-serial-parse-drops'),
  serialOversizeDrops: document.getElementById('status-serial-oversize-drops'),
  oscDrops: document.getElementById('status-osc-drops'),
  midiDrops: document.getElementById('status-midi-drops'),
  feedbackSuppressed: document.getElementById('status-feedback-suppressed'),
};

const sessionNodes = {
  deviceName: document.getElementById('device-name'),
  firmware: document.getElementById('device-fw'),
  schema: document.getElementById('device-schema'),
  schemaSource: document.getElementById('device-schema-source'),
  power: document.getElementById('device-power'),
  ledCap: document.getElementById('device-led-cap'),
  rail: document.getElementById('device-rail'),
  display: document.getElementById('device-display'),
  brownouts: document.getElementById('device-brownouts'),
  eeprom: document.getElementById('device-eeprom'),
  freeRam: document.getElementById('device-free-ram'),
  configValidation: document.getElementById('device-config-validation'),
  authority: document.getElementById('device-authority'),
  draftState: document.getElementById('device-draft-state'),
  lastApply: document.getElementById('device-last-apply'),
};

const consoleState = {
  bridge: null,
  rawLines: [],
  structuredEvents: [],
  presets: [],
  activePresetId: '',
  midiToOscMappings: [],
  outboundMidiMappings: [],
  customSetups: [],
  activeCustomSetupId: '',
  loadedCustomSetupId: '',
};
window.__MN42_BRIDGE_CONSOLE_STATE = consoleState;

let latestPorts = [];
let latestMidiPorts = { inputs: [], outputs: [] };
let rawSocket = null;
let eventSocket = null;
let soundcheck = null;
let mappingLearn = null;
let hostFormDirty = false;
let customSetupNotice = '';
const SOUNDCHECK_WINDOW_MS = 15_000;
const MAPPING_LEARN_WINDOW_MS = 10_000;

function loadSavedConfig() {
  try {
    return JSON.parse(localStorage.getItem(STORAGE_KEY) || '{}');
  } catch (_) {
    return {};
  }
}

function saveConfig(values) {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(values));
  } catch (_) {
    // no-op
  }
}

function saveMode(mode) {
  try {
    localStorage.setItem(MODE_KEY, mode);
  } catch (_) {
    // no-op
  }
}

function loadMode() {
  try {
    return localStorage.getItem(MODE_KEY) || 'setup';
  } catch (_) {
    return 'setup';
  }
}

function selectedCustomSetup() {
  return consoleState.customSetups.find(
    (setup) => setup.id === consoleState.activeCustomSetupId,
  );
}

function loadedCustomSetup() {
  return consoleState.customSetups.find(
    (setup) => setup.id === consoleState.loadedCustomSetupId,
  );
}

function loadCustomSetups() {
  try {
    const raw = JSON.parse(localStorage.getItem(CUSTOM_SETUPS_KEY) || 'null');
    const parsed = parseHostSetupEnvelope(raw);
    return parsed.valid ? parsed.setups : [];
  } catch (_) {
    return [];
  }
}

function persistCustomSetups() {
  try {
    const envelope = createHostSetupEnvelope(consoleState.customSetups);
    localStorage.setItem(CUSTOM_SETUPS_KEY, JSON.stringify(envelope));
    return true;
  } catch (_) {
    return false;
  }
}

function renderCustomSetupStatus() {
  const selected = selectedCustomSetup();
  loadCustomSetupButton.disabled = !selected;
  deleteCustomSetupButton.disabled = !selected;
  if (!selected) {
    customSetupStatus.textContent =
      customSetupNotice || 'No custom setup selected.';
    return;
  }
  const changed =
    hostSetupConfigFingerprint(formValues()) !==
      hostSetupConfigFingerprint(selected.config) ||
    customSetupName.value.trim() !== selected.name ||
    customSetupProfile.value.trim() !== (selected.suggestedDeviceProfile || '') ||
    customSetupNotes.value.trim() !== (selected.notes || '');
  customSetupStatus.textContent = changed
    ? `${selected.name} selected · current form differs from the saved setup.`
    : customSetupNotice || `${selected.name} selected · no unsaved setup changes.`;
}

function renderCustomSetups({ syncName = false } = {}) {
  customSetupSelect.textContent = '';
  const empty = document.createElement('option');
  empty.value = '';
  empty.textContent = 'New setup';
  customSetupSelect.appendChild(empty);
  [...consoleState.customSetups]
    .sort((a, b) => a.name.localeCompare(b.name))
    .forEach((setup) => {
      const option = document.createElement('option');
      option.value = setup.id;
      option.textContent = setup.name;
      customSetupSelect.appendChild(option);
    });
  if (selectedCustomSetup()) {
    customSetupSelect.value = consoleState.activeCustomSetupId;
    if (syncName) {
      customSetupName.value = selectedCustomSetup().name;
      customSetupProfile.value = selectedCustomSetup().suggestedDeviceProfile || '';
      customSetupNotes.value = selectedCustomSetup().notes || '';
    }
  } else {
    consoleState.activeCustomSetupId = '';
    customSetupSelect.value = '';
    if (syncName) {
      customSetupName.value = '';
      customSetupProfile.value = '';
      customSetupNotes.value = '';
    }
  }
  exportCustomSetupsButton.disabled = !consoleState.customSetups.length;
  renderCustomSetupStatus();
}

function beginNewCustomSetup() {
  consoleState.activeCustomSetupId = '';
  customSetupName.value = '';
  customSetupProfile.value = '';
  customSetupNotes.value = '';
  customSetupNotice = 'Enter a name, then save the current performance setup.';
  renderCustomSetups();
  customSetupName.focus();
}

function customSetupId() {
  if (window.crypto?.randomUUID) return window.crypto.randomUUID();
  return `setup-${Date.now()}-${Math.random().toString(16).slice(2)}`;
}

function saveCurrentCustomSetup() {
  const name = customSetupName.value.trim().slice(0, 80);
  const config = normalizeHostSetupConfig(formValues());
  if (!name || !config) {
    customSetupNotice =
      'Name the setup and provide valid OSC host, bind, and port values.';
    renderCustomSetupStatus();
    return;
  }
  const current = selectedCustomSetup();
  if (
    current &&
    !window.confirm(`Replace saved setup “${current.name}” with the current form?`)
  ) {
    return;
  }
  const now = new Date().toISOString();
  const setup = {
    id: current?.id || customSetupId(),
    name,
    suggestedDeviceProfile: customSetupProfile.value.trim().slice(0, 80) || null,
    notes: customSetupNotes.value.trim().slice(0, 500) || null,
    createdAt: current?.createdAt || now,
    updatedAt: now,
    config,
  };
  const previousSetups = consoleState.customSetups;
  const previousActiveId = consoleState.activeCustomSetupId;
  consoleState.customSetups = current
    ? consoleState.customSetups.map((entry) =>
        entry.id === current.id ? setup : entry,
      )
    : [...consoleState.customSetups, setup];
  consoleState.activeCustomSetupId = setup.id;
  if (!persistCustomSetups()) {
    consoleState.customSetups = previousSetups;
    consoleState.activeCustomSetupId = previousActiveId;
    customSetupNotice = 'Browser storage rejected the setup; export existing setups before retrying.';
    renderCustomSetups();
    return;
  }
  customSetupNotice = `Saved “${name}” in this browser.`;
  renderCustomSetups({ syncName: true });
}

function loadSelectedCustomSetup() {
  const setup = selectedCustomSetup();
  if (!setup) return;
  populateForm(setup.config);
  consoleState.loadedCustomSetupId = setup.id;
  customSetupProfile.value = setup.suggestedDeviceProfile || '';
  customSetupNotes.value = setup.notes || '';
  hostFormDirty = true;
  saveConfig(formValues());
  customSetupNotice = consoleState.bridge?.running
    ? `Loaded “${setup.name}” into the form. Stop and start the Bridge to apply transport changes.`
    : `Loaded “${setup.name}” into the form. Start the Bridge when ready.`;
  renderCustomSetupStatus();
}

function deleteSelectedCustomSetup() {
  const setup = selectedCustomSetup();
  if (!setup || !window.confirm(`Delete browser-local setup “${setup.name}”?`)) {
    return;
  }
  const previousSetups = consoleState.customSetups;
  const previousLoadedId = consoleState.loadedCustomSetupId;
  consoleState.customSetups = consoleState.customSetups.filter(
    (entry) => entry.id !== setup.id,
  );
  consoleState.activeCustomSetupId = '';
  if (consoleState.loadedCustomSetupId === setup.id) {
    consoleState.loadedCustomSetupId = '';
  }
  if (!persistCustomSetups()) {
    consoleState.customSetups = previousSetups;
    consoleState.activeCustomSetupId = setup.id;
    consoleState.loadedCustomSetupId = previousLoadedId;
    customSetupNotice = 'Browser storage rejected the deletion.';
    renderCustomSetups({ syncName: true });
    return;
  }
  customSetupNotice = `Deleted “${setup.name}”.`;
  renderCustomSetups({ syncName: true });
}

function exportCustomSetups() {
  if (!consoleState.customSetups.length) return;
  const envelope = createHostSetupEnvelope(consoleState.customSetups);
  const blob = new Blob([`${JSON.stringify(envelope, null, 2)}\n`], {
    type: 'application/json',
  });
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = 'mn42-bridge-performance-setups.json';
  anchor.click();
  URL.revokeObjectURL(url);
  customSetupNotice = `Exported ${envelope.setups.length} browser-local setup${
    envelope.setups.length === 1 ? '' : 's'
  }.`;
  renderCustomSetupStatus();
}

async function importCustomSetups(file) {
  try {
    if (Number(file?.size) > 1024 * 1024) {
      throw new Error('Setup import exceeds the 1 MiB limit.');
    }
    const parsed = parseHostSetupEnvelope(JSON.parse(await file.text()));
    if (!parsed.valid || !parsed.setups.length) {
      throw new Error('No valid version 1 MN42 Performance Setups found.');
    }
    if (
      !window.confirm(
        `Import ${parsed.setups.length} setup${
          parsed.setups.length === 1 ? '' : 's'
        }? Matching setup IDs will be replaced.`,
      )
    ) {
      return;
    }
    const merged = new Map(
      consoleState.customSetups.map((setup) => [setup.id, setup]),
    );
    parsed.setups.forEach((setup) => merged.set(setup.id, setup));
    const previousSetups = consoleState.customSetups;
    consoleState.customSetups = [...merged.values()];
    if (!persistCustomSetups()) {
      consoleState.customSetups = previousSetups;
      throw new Error('Browser storage rejected the imported setups.');
    }
    customSetupNotice = `Imported ${parsed.setups.length} setup${
      parsed.setups.length === 1 ? '' : 's'
    }${parsed.rejected ? `; rejected ${parsed.rejected} invalid entries` : ''}.`;
    renderCustomSetups();
  } catch (error) {
    customSetupNotice = `Import failed: ${error.message}`;
    renderCustomSetupStatus();
  } finally {
    importCustomSetupsInput.value = '';
  }
}

function clone(value) {
  return value == null ? value : JSON.parse(JSON.stringify(value));
}

function pushLimited(list, value, limit) {
  list.push(value);
  if (list.length > limit) {
    list.splice(0, list.length - limit);
  }
}

function formValues() {
  const data = new FormData(form);
  const allowFeedbackLoopsField = form.elements.namedItem('allowFeedbackLoops');
  return {
    serialName: String(data.get('serialName') || '').trim(),
    midiLabel: String(data.get('midiLabel') || '').trim(),
    midiDestinationName: String(data.get('midiDestinationName') || '').trim(),
    oscHost: String(data.get('oscHost') || '').trim(),
    oscPort: Number(data.get('oscPort') || 9000),
    oscDestinationName: String(data.get('oscDestinationName') || '').trim(),
    oscListen: Number(data.get('oscListen') || 9000),
    oscBind: String(data.get('oscBind') || '').trim(),
    feedbackWindowMs: Number(data.get('feedbackWindowMs') || 120),
    rtP95TargetMs: Number(data.get('rtP95TargetMs') || 10),
    rtJitterP95TargetMs: Number(data.get('rtJitterP95TargetMs') || 5),
    alertSuppressionMs: Number(data.get('alertSuppressionMs') || 3000),
    allowFeedbackLoops:
      Boolean(allowFeedbackLoopsField) &&
      Boolean(allowFeedbackLoopsField.checked),
    midiToOscMappings: clone(consoleState.midiToOscMappings),
    midiTelemetryMode: String(data.get('midiTelemetryMode') || 'legacy'),
    outboundMidiMappings: clone(consoleState.outboundMidiMappings),
  };
}

function populateForm(values) {
  if (!values || typeof values !== 'object') return;
  for (const [key, value] of Object.entries(values)) {
    const field = form.elements.namedItem(key);
    if (!field || value === undefined || value === null) continue;
    if (field.type === 'checkbox') {
      field.checked = Boolean(value);
      continue;
    }
    field.value = String(value);
  }
  if (Array.isArray(values.midiToOscMappings)) {
    consoleState.midiToOscMappings = clone(values.midiToOscMappings);
    renderMappingList();
    renderMappingOutput(consoleState.bridge || {});
  }
  if (Array.isArray(values.outboundMidiMappings)) {
    consoleState.outboundMidiMappings = clone(values.outboundMidiMappings);
    if (outboundMidiMappingsInput) {
      outboundMidiMappingsInput.value = JSON.stringify(values.outboundMidiMappings, null, 2);
    }
  }
}

function formatWhen(isoString) {
  if (!isoString) return 'none yet';
  const date = new Date(isoString);
  if (Number.isNaN(date.getTime())) return String(isoString);
  return date.toLocaleString();
}

function formatTimestampMs(raw) {
  if (raw === undefined || raw === null) return 'none yet';
  const value = Number(raw);
  return Number.isFinite(value) ? `${Math.trunc(value)} ms` : String(raw);
}

function formatMetricMs(raw) {
  if (raw === undefined || raw === null) return 'n/a';
  const value = Number(raw);
  return Number.isFinite(value) ? `${Math.trunc(value)} ms` : 'n/a';
}

function formatBooleanStatus(value, trueLabel = 'ok', falseLabel = 'warn') {
  if (value === true) return trueLabel;
  if (value === false) return falseLabel;
  return '-';
}

function renderOperatorStatus(node, description) {
  if (!node) return;
  node.textContent = description?.label || '-';
  node.classList.remove('status-ok', 'status-warn', 'status-error', 'status-muted');
  if (description?.status) node.classList.add(`status-${description.status}`);
}

function formatJson(value) {
  return JSON.stringify(value ?? null, null, 2);
}

function logsToText(logs) {
  if (!Array.isArray(logs) || !logs.length) {
    return 'Waiting for bridge activity.';
  }
  return logs
    .map((entry) => {
      const prefix = `[${entry.level || 'info'}]`;
      const extra = entry.extra ? ` ${JSON.stringify(entry.extra)}` : '';
      return `${entry.at || ''} ${prefix} ${
        entry.message || ''
      }${extra}`.trim();
    })
    .join('\n');
}

function formatRoute(route) {
  if (!route || typeof route !== 'object') return '';
  const head = `${route.hostTimestampMs || ''} ${route.flow || 'route'} ${
    route.kind || ''
  }`;
  const slotIndex = Number(route.slot);
  const slotMetadata = Number.isInteger(slotIndex)
    ? consoleState.bridge?.appDisplayMetadata?.slots?.find((entry) => entry.index === slotIndex)
    : null;
  const advisoryLabel = slotMetadata?.routeDescription || slotMetadata?.label || '';
  const tail = [route.traceId, advisoryLabel, route.source, route.destination]
    .filter(Boolean)
    .join(' · ');
  return [head, tail].filter(Boolean).join(' — ');
}

function renderRouteOutput(routes = []) {
  if (!routeOutput) return;
  if (!Array.isArray(routes) || !routes.length) {
    routeOutput.textContent = 'No route traces yet.';
    return;
  }
  routeOutput.textContent = routes
    .slice(-40)
    .map((entry) => formatRoute(entry))
    .join('\n');
}

function finishSoundcheck() {
  if (!soundcheck) return;
  window.clearTimeout(soundcheck.timer);
  const observed = soundcheck.observed;
  if (!soundcheck.detection) {
    soundcheckStatus.textContent =
      'No slot-value change detected. Confirm live telemetry and try again.';
  } else if (observed.has('deviceOsc') && observed.has('deviceMidi')) {
    soundcheckStatus.textContent =
      'Soundcheck complete: the detected slot change reached both OSC and MIDI.';
  } else {
    const seen = [
      observed.has('deviceOsc') ? 'OSC observed' : 'OSC not observed',
      observed.has('deviceMidi') ? 'MIDI observed' : 'MIDI not observed',
    ];
    soundcheckStatus.textContent = `Soundcheck finished: ${seen.join('; ')}.`;
  }
  soundcheck = null;
  startSoundcheckButton.textContent = 'Start passive soundcheck';
  updateButtons(consoleState.bridge || {});
}

function renderRoutingHeartbeat(routes = []) {
  const heartbeat = describeRouteHeartbeats(routes);
  const destinations = describeRoutingDestinations(
    consoleState.bridge?.config || formValues(),
  );
  Object.entries(routeHeartbeatNodes).forEach(([lane, card]) => {
    if (!card) return;
    const description = heartbeat[lane];
    const label = card.querySelector('span');
    if (label) label.textContent = destinations[lane];
    renderOperatorStatus(card.querySelector('strong'), description);
    card.classList.toggle('is-recent', Boolean(description?.recent));
  });

  if (!soundcheck?.detection) return;
  soundcheck.observed = observedSoundcheckLanes(routes, soundcheck.detection);
  if (
    soundcheck.observed.has('deviceOsc') &&
    soundcheck.observed.has('deviceMidi')
  ) {
    finishSoundcheck();
  }
}

function latestSlotTelemetry() {
  for (let index = consoleState.rawLines.length - 1; index >= 0; index -= 1) {
    const telemetry = parseSlotTelemetryLine(consoleState.rawLines[index]);
    if (telemetry) return telemetry;
  }
  return null;
}

function observeSoundcheckTelemetry(line) {
  if (!soundcheck || soundcheck.detection) return;
  const telemetry = parseSlotTelemetryLine(line);
  if (!telemetry) return;
  if (!soundcheck.baseline) {
    soundcheck.baseline = telemetry.slots;
    soundcheckStatus.textContent =
      'Telemetry baseline captured. Move one stable, unmodulated hardware control now.';
    return;
  }
  const changed = changedSlotIndices(soundcheck.baseline, telemetry.slots);
  soundcheck.baseline = telemetry.slots;
  if (!changed.length) return;
  soundcheck.detection = {
    traceId: telemetry.traceId,
    detectedAt: Date.now(),
    startedAt: soundcheck.startedAt,
  };
  soundcheckStatus.textContent = `Slot ${changed[0] + 1} telemetry changed. Confirming OSC and MIDI routes…`;
  renderRoutingHeartbeat(consoleState.bridge?.routes || []);
}

function startPassiveSoundcheck() {
  if (soundcheck) return;
  const baseline = latestSlotTelemetry();
  soundcheck = {
    active: true,
    startedAt: Date.now(),
    baseline: baseline?.slots || null,
    detection: null,
    observed: new Set(),
    timer: window.setTimeout(finishSoundcheck, SOUNDCHECK_WINDOW_MS),
  };
  startSoundcheckButton.textContent = 'Soundcheck listening…';
  soundcheckStatus.textContent = baseline
    ? 'Listening. Move one stable, unmodulated hardware control now.'
    : 'Waiting for a telemetry baseline. Keep the device connected.';
  updateButtons(consoleState.bridge || {});
}

function renderRawSerialOutput() {
  if (!rawSerialOutput) return;
  rawSerialOutput.textContent = consoleState.rawLines.length
    ? consoleState.rawLines.join('\n')
    : 'Raw serial lane idle.';
}

function renderStateJson(state) {
  if (!stateJsonOutput) return;
  stateJsonOutput.textContent = formatJson({
    bridge: state,
    structuredEvents: consoleState.structuredEvents.slice(-12),
  });

  if (stagePerformanceSetup) {
    const setup = loadedCustomSetup();
    const details = setup
      ? [
          `Performance setup: ${setup.name}`,
          setup.suggestedDeviceProfile
            ? `suggested device profile: ${setup.suggestedDeviceProfile}`
            : '',
          setup.notes || '',
        ].filter(Boolean)
      : ['Performance setup: unsaved current routing'];
    const metadata = consoleState.bridge?.appDisplayMetadata;
    const activeProfileLabel = metadata?.profileLabels?.[metadata?.activeProfile];
    if (activeProfileLabel) details.push(`App profile: ${activeProfileLabel}`);
    stagePerformanceSetup.textContent = details.join(' · ');
  }
}

function renderMappingOutput(state = {}) {
  if (!mappingOutput) return;
  const config = state.config || {};
  mappingOutput.textContent = formatJson({
    feedbackWindowMs: config.feedbackWindowMs ?? null,
    allowFeedbackLoops: Boolean(config.allowFeedbackLoops),
    midiLabel: config.midiLabel ?? null,
    activeMidiToOscMappings: config.midiToOscMappings || [],
    formMidiToOscMappings: consoleState.midiToOscMappings || [],
    midiTelemetryMode: config.midiTelemetryMode || 'legacy',
    outboundMidiMappings: config.outboundMidiMappings || [],
  });
}

function mappingFormValue() {
  const data = new FormData(addMappingForm);
  return {
    id: String(data.get('id') || '').trim(),
    controller: Number.parseInt(data.get('controller'), 10),
    channel: data.get('channel')
      ? Number.parseInt(data.get('channel'), 10)
      : null,
    address: String(data.get('address') || '').trim(),
    valueMode: data.get('valueMode') || 'raw',
  };
}

function renderMappingPreview() {
  if (!mappingPreview) return;
  const mapping = mappingFormValue();
  if (!mapping.id || !Number.isInteger(mapping.controller) || !mapping.address) {
    mappingPreview.textContent = 'No mapping ready yet.';
    return;
  }
  mappingPreview.textContent = `${mapping.id}: MIDI Ch ${
    mapping.channel || 'any'
  } CC ${mapping.controller} → ${mapping.address} (${mapping.valueMode})`;
}

function renderRecentOscAddresses(routes = []) {
  if (!recentOscAddressSelect) return;
  const selected = recentOscAddressSelect.value;
  const addresses = recentOscAddresses(routes);
  recentOscAddressSelect.textContent = '';
  const placeholder = document.createElement('option');
  placeholder.value = '';
  placeholder.textContent = addresses.length
    ? 'Choose a recent OSC address'
    : 'No OSC addresses observed yet';
  recentOscAddressSelect.appendChild(placeholder);
  addresses.forEach((address) => {
    const option = document.createElement('option');
    option.value = address;
    option.textContent = address;
    recentOscAddressSelect.appendChild(option);
  });
  if (addresses.includes(selected)) recentOscAddressSelect.value = selected;
}

function finishMappingLearn(message) {
  if (!mappingLearn) return;
  window.clearTimeout(mappingLearn.timer);
  mappingLearn = null;
  startMappingLearnButton.textContent = 'Listen for MIDI CC';
  mappingLearnStatus.textContent = message;
  updateButtons(consoleState.bridge || {});
}

function observeMappingLearnRoutes(routes = []) {
  if (!mappingLearn) return;
  const learned = latestLearnableMidiCc(routes, mappingLearn.startedAt);
  if (!learned) return;
  addMappingForm.elements.namedItem('controller').value = String(
    learned.controller,
  );
  addMappingForm.elements.namedItem('channel').value = String(learned.channel);
  const idField = addMappingForm.elements.namedItem('id');
  if (!idField.value.trim()) {
    idField.value = `cc-${learned.controller}-ch-${learned.channel}`;
  }
  renderMappingPreview();
  finishMappingLearn(
    `Captured channel ${learned.channel}, CC ${learned.controller}, value ${
      learned.value ?? 'unknown'
    }. Choose or type an OSC address, then review the mapping.`,
  );
}

function startMappingLearn() {
  if (mappingLearn) return;
  mappingLearn = {
    startedAt: Date.now(),
    timer: window.setTimeout(
      () =>
        finishMappingLearn(
          'No inbound MIDI CC detected. Check the selected MIDI input and try again.',
        ),
      MAPPING_LEARN_WINDOW_MS,
    ),
  };
  startMappingLearnButton.textContent = 'Listening for MIDI…';
  mappingLearnStatus.textContent =
    'Listening. Move one MIDI knob, fader, or pedal now.';
  updateButtons(consoleState.bridge || {});
}

async function commitMappings(mappings) {
  const payload = await api('/api/mappings', {
    method: 'POST',
    body: JSON.stringify({ midiToOscMappings: mappings }),
  });
  updateStatus(payload.state);
  consoleState.midiToOscMappings = clone(
    payload.state?.config?.midiToOscMappings || [],
  );
  renderMappingList();
  renderMappingOutput(payload.state || {});
  saveConfig(formValues());
  renderCustomSetupStatus();
  return consoleState.midiToOscMappings;
}

function renderMappingList() {
  if (!mappingListBody) return;
  mappingListBody.textContent = '';
  const mappings = consoleState.midiToOscMappings || [];
  if (!mappings.length) {
    const row = document.createElement('tr');
    const cell = document.createElement('td');
    cell.colSpan = 5;
    cell.style.textAlign = 'center';
    cell.textContent = 'No mappings configured yet.';
    row.appendChild(cell);
    mappingListBody.appendChild(row);
    return;
  }
  mappings.forEach((mapping, index) => {
    const row = document.createElement('tr');
    const values = [
      mapping.id || `mapping-${index + 1}`,
      `CC ${mapping.controller}${
        mapping.channel ? ` (Ch ${mapping.channel})` : ''
      }`,
      mapping.address,
      mapping.valueMode,
    ];
    values.forEach((value) => {
      const cell = document.createElement('td');
      cell.textContent = String(value ?? '');
      row.appendChild(cell);
    });
    const actions = document.createElement('td');
    actions.className = 'actions';
    const remove = document.createElement('button');
    remove.className = 'remove-mapping';
    remove.dataset.index = String(index);
    remove.type = 'button';
    remove.textContent = 'Remove';
    actions.appendChild(remove);
    row.appendChild(actions);
    mappingListBody.appendChild(row);
  });

  mappingListBody.querySelectorAll('.remove-mapping').forEach((btn) => {
    btn.addEventListener('click', async () => {
      const index = parseInt(btn.dataset.index, 10);
      const mapping = consoleState.midiToOscMappings[index];
      if (!window.confirm(`Remove mapping “${mapping?.id || index + 1}”?`)) {
        return;
      }
      try {
        await commitMappings(
          consoleState.midiToOscMappings.filter(
            (_, entryIndex) => entryIndex !== index,
          ),
        );
        summaryStatus.textContent =
          'Mapping removed without restarting routing.';
      } catch (error) {
        summaryStatus.textContent = `Mapping removal failed: ${error.message}`;
      }
    });
  });
}

function wsUrl(pathname = '/ws') {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  return `${protocol}//${window.location.host}${pathname}`;
}

function updateButtons(state) {
  const running = Boolean(state?.running);
  startButton.disabled = running;
  stopButton.disabled = !running;
  openConfiguratorButton.disabled = !running;
  if (stageOpenConfiguratorButton) {
    stageOpenConfiguratorButton.disabled = !running;
  }
  if (startSoundcheckButton) {
    startSoundcheckButton.disabled =
      !running || !state?.serialConnected || Boolean(soundcheck);
  }
  if (startMappingLearnButton) {
    startMappingLearnButton.disabled = !running || Boolean(mappingLearn);
  }
}

function renderAlertHistory(alerts = []) {
  if (!alertHistory) return;
  alertHistory.textContent = '';
  if (!Array.isArray(alerts) || !alerts.length) {
    const item = document.createElement('li');
    item.className = 'alerts-empty';
    item.textContent = 'No active alerts.';
    alertHistory.appendChild(item);
    return;
  }
  alerts
    .slice(-12)
    .reverse()
    .forEach((alert) => {
      const item = document.createElement('li');
      const severity = alert?.severity || 'warn';
      item.className = `alert-item severity-${severity}`;
      item.textContent = `[${severity}] ${formatWhen(alert?.at)} — ${
        alert?.message || alert?.code || 'alert'
      }`;
      alertHistory.appendChild(item);
    });
}

function updateMode(mode) {
  modeTabs.forEach((button) => {
    button.classList.toggle('is-active', button.dataset.mode === mode);
  });
  modeViews.forEach((view) => {
    const active = view.dataset.modeView === mode;
    view.classList.toggle('is-active', active);
    view.hidden = !active;
  });
  modeScopedActions.forEach((action) => {
    action.hidden = !isActionVisibleInMode(action.dataset.consoleModes, mode);
  });
  saveMode(mode);
}

function updateSession(session = {}) {
  sessionNodes.deviceName.textContent =
    session?.firmwareIdentity?.device_name ||
    session?.manifest?.device_name ||
    '-';
  sessionNodes.firmware.textContent =
    session?.firmwareIdentity?.fw_version ||
    session?.manifest?.fw_version ||
    '-';
  sessionNodes.schema.textContent = String(
    session?.firmwareIdentity?.schema_version ??
      session?.manifest?.schema_version ??
      session?.schema?.schema_version ??
      '-',
  );
  sessionNodes.schemaSource.textContent = session?.schemaSource || '-';
  sessionNodes.power.textContent = session?.powerSafety?.power_profile || '-';
  sessionNodes.ledCap.textContent =
    session?.powerSafety?.led_brightness_cap ?? '-';
  sessionNodes.rail.textContent =
    session?.powerSafety?.rail_topology_verified === true
      ? 'verified'
      : session?.powerSafety?.rail_topology_verified === false
        ? 'unverified'
        : '-';
  const health = session?.hardwareHealth || {};
  const displayOk = formatBooleanStatus(health.display_ok, 'ok', 'degraded');
  const displayStatus = health.display_status || '-';
  sessionNodes.display.textContent =
    displayOk === '-' && displayStatus === '-'
      ? '-'
      : `${displayOk} (${displayStatus})`;
  sessionNodes.brownouts.textContent = health.brownout_count ?? '-';
  const primary = formatBooleanStatus(
    health.eeprom_primary_valid,
    'primary ok',
    'primary bad',
  );
  const backup = formatBooleanStatus(
    health.eeprom_backup_valid,
    'backup ok',
    'backup bad',
  );
  sessionNodes.eeprom.textContent =
    primary === '-' && backup === '-'
      ? health.eeprom_last_load || '-'
      : `${primary}, ${backup}`;
  sessionNodes.freeRam.textContent = health.free_ram ?? '-';
  const validation = describeConfigValidation(session?.configValidation);
  const authority = describeAuthority(session);
  const draft = describeDraft(session);
  renderOperatorStatus(sessionNodes.configValidation, validation);
  renderOperatorStatus(sessionNodes.authority, authority);
  renderOperatorStatus(sessionNodes.draftState, draft);
  const recoveryRequired = validation.recoveryRequired || draft.recoveryRequired;
  if (stageRecoveryNeeded) {
    stageRecoveryNeeded.hidden = !recoveryRequired;
    stageRecoveryNeeded.textContent = validation.recoveryRequired
      ? 'Device configuration export failed validation. Open the App to inspect the contract failure before relying on cached state.'
      : recoveryRequired
        ? `${draft.label}. Open the App to resolve staged or uncertain configuration state.`
        : '';
  }
  if (stageOpenConfiguratorButton) {
    stageOpenConfiguratorButton.textContent = recoveryRequired
      ? 'Open App to resolve'
      : 'Open configurator';
  }
  const lastApply = session?.lastApplyResult;
  sessionNodes.lastApply.textContent = lastApply
    ? `${lastApply.status || 'unknown'}${
        lastApply.checksum
          ? ` (${String(lastApply.checksum).slice(0, 8)}…)`
          : ''
      }`
    : 'none';
}

function updateStatus(state) {
  consoleState.bridge = clone(state);
  window.__MN42_BRIDGE_CONSOLE_STATE = consoleState;

  const config = state?.config || {};
  const counters = state?.counters || {};
  const timing = state?.timing || {};
  const roundTrip = state?.performance?.roundTrip || {};
  const performanceHealth = state?.performance?.health || {};
  const activeAlerts = selectActiveAlerts(state);
  const telemetryFreshness = formatTelemetryFreshness(state);
  const configValidation = describeConfigValidation(
    state?.deviceSession?.configValidation,
  );

  statusNodes.running.textContent = state?.running ? 'running' : 'stopped';
  statusNodes.serial.textContent = state?.serialConnected
    ? 'connected'
    : 'disconnected';
  statusNodes.ready.textContent = state?.ready ? 'ready' : 'awaiting handshake';
  statusNodes.osc.textContent = `${config.oscHost || '127.0.0.1'}:${
    config.oscPort || 9000
  }`;
  statusNodes.oscListen.textContent = `${config.oscBind || '127.0.0.1'}:${
    config.oscListen || 9000
  }`;
  statusNodes.midi.textContent = config.midiLabel || 'MN42 Bridge';
  renderOperatorStatus(statusNodes.telemetry, telemetryFreshness);
  statusNodes.lastRoute.textContent = formatWhen(state?.lastRouteAt);
  statusNodes.lastTrace.textContent = state?.lastRouteTraceId || 'none';
  statusNodes.sourceTs.textContent = formatTimestampMs(
    timing.lastSerialSourceTimestampMs,
  );
  statusNodes.sourceSkew.textContent =
    timing.lastSerialSkewMs === null || timing.lastSerialSkewMs === undefined
      ? 'n/a'
      : `${Math.trunc(Number(timing.lastSerialSkewMs))} ms`;
  statusNodes.roundTripSamples.textContent = String(roundTrip.sampleCount || 0);
  statusNodes.roundTripPending.textContent = String(roundTrip.pending || 0);
  statusNodes.roundTripLast.textContent = formatMetricMs(roundTrip.lastMs);
  statusNodes.roundTripP50.textContent = formatMetricMs(roundTrip.p50Ms);
  statusNodes.roundTripP95.textContent = formatMetricMs(roundTrip.p95Ms);
  statusNodes.roundTripJitterP95.textContent = formatMetricMs(
    roundTrip.jitterP95Ms,
  );
  statusNodes.roundTripHealth.textContent = performanceHealth.status || 'n/a';
  statusNodes.alertCount.textContent = String(activeAlerts.length);
  statusNodes.alertTop.textContent = activeAlerts.length
    ? `${activeAlerts[0].severity || 'warn'}: ${
        activeAlerts[0].message || activeAlerts[0].code || 'alert'
      }`
    : 'none';
  statusNodes.error.textContent = state?.lastError || 'none';
  statusNodes.serialParseDrops.textContent = String(
    counters.serialParseErrors || 0,
  );
  statusNodes.serialOversizeDrops.textContent = String(
    counters.serialOversizeDrops || 0,
  );
  statusNodes.oscDrops.textContent = String(counters.badOscCmdDrops || 0);
  statusNodes.midiDrops.textContent = String(counters.badMidiCmdDrops || 0);
  statusNodes.feedbackSuppressed.textContent = String(
    counters.feedbackSuppressed || 0,
  );

  summaryStatus.textContent = state?.running
    ? configValidation.recoveryRequired
      ? 'Bridge running, but the device configuration export failed validation.'
      : state?.ready
        ? 'Bridge live and cached device session ready.'
        : 'Bridge running. Waiting for handshake/schema/config cache.'
    : 'Bridge idle.';

  logOutput.textContent = logsToText(state?.logs);
  renderAlertHistory(activeAlerts);
  updateButtons(state);
  updateSession(state?.deviceSession || {});
  renderRouteOutput(state?.routes || []);
  renderRoutingHeartbeat(state?.routes || []);
  renderRecentOscAddresses(state?.routes || []);
  observeMappingLearnRoutes(state?.routes || []);
  renderRawSerialOutput();
  renderStateJson(state);
  renderMappingOutput(state);
}

function scoreSerialPort(port) {
  const path = String(port?.path || '').toLowerCase();
  const manufacturer = String(port?.manufacturer || '').toLowerCase();
  const label = `${path} ${manufacturer}`;
  if (!path) return -1;
  if (label.includes('teensy')) return 100;
  if (path.includes('usbmodem')) return 80;
  if (path.includes('ttyacm') || path.includes('ttyusb')) return 70;
  if (path.includes('usbserial')) return 60;
  if (label.includes('bluetooth') || label.includes('wlan')) return 0;
  return 10;
}

function displaySerialPath(pathname) {
  const value = String(pathname || '').trim();
  if (
    value.startsWith('/dev/tty.usbmodem') ||
    value.startsWith('/dev/tty.usbserial')
  ) {
    return value.replace('/dev/tty.', '/dev/cu.');
  }
  return value;
}

function preferredSerialPort(ports = []) {
  return [...ports]
    .filter((port) => port?.path)
    .sort((a, b) => scoreSerialPort(b) - scoreSerialPort(a))[0];
}

function shouldReplaceSerialPath(current, ports = []) {
  const value = String(current || '').trim();
  if (!value) return true;
  if (value === '/dev/ttyACM0') return true;
  return !ports.some(
    (port) => port?.path === value || displaySerialPath(port?.path) === value,
  );
}

function applyPreferredSerialPort(ports = latestPorts) {
  const field = form.elements.namedItem('serialName');
  if (!field || !shouldReplaceSerialPath(field.value, ports)) return false;
  const preferred = preferredSerialPort(ports);
  if (!preferred?.path) return false;
  field.value = displaySerialPath(preferred.path);
  saveConfig(formValues());
  return true;
}

function renderPorts(ports) {
  latestPorts = Array.isArray(ports) ? ports : [];
  portsList.textContent = '';
  portsDatalist.textContent = '';
  if (!latestPorts.length) {
    const item = document.createElement('li');
    item.textContent =
      'No serial ports detected yet. You can still type a port path manually.';
    portsList.appendChild(item);
    return;
  }
  latestPorts.forEach((port) => {
    const value = displaySerialPath(port.path);
    const option = document.createElement('option');
    option.value = value;
    portsDatalist.appendChild(option);
    const item = document.createElement('li');
    item.textContent = `${value}${
      port.manufacturer ? ` — ${port.manufacturer}` : ''
    }`;
    portsList.appendChild(item);
  });
}

function describeMidiPort(port) {
  const parts = [port?.name || port?.id || 'Unknown MIDI port'];
  if (port?.manufacturer) parts.push(port.manufacturer);
  if (port?.engine) parts.push(port.engine);
  return parts.join(' — ');
}

function midiPortName(port) {
  return String(port?.name || port?.id || '').trim();
}

function commonMidiPortNames({ inputs = [], outputs = [] } = {}) {
  const inputNames = new Set(inputs.map(midiPortName).filter(Boolean));
  return outputs
    .map(midiPortName)
    .filter((name) => name && inputNames.has(name));
}

function scoreMidiPortName(name) {
  const value = String(name || '').toLowerCase();
  if (!value) return -1;
  if (value.includes('iac')) return 100;
  if (value.includes('mn42') || value.includes('moarknobs')) return 90;
  if (value.includes('loopmidi')) return 85;
  if (value.includes('teensy')) return 20;
  if (value.includes('dls synth')) return 0;
  return 50;
}

function preferredMidiPortName(ports = latestMidiPorts) {
  return commonMidiPortNames(ports).sort(
    (a, b) => scoreMidiPortName(b) - scoreMidiPortName(a),
  )[0];
}

function shouldReplaceMidiLabel(current, ports = latestMidiPorts) {
  const value = String(current || '').trim();
  const available = commonMidiPortNames(ports);
  if (!available.length) return false;
  if (!value) return true;
  return !available.includes(value);
}

function applyPreferredMidiPort(ports = latestMidiPorts) {
  const field = form.elements.namedItem('midiLabel');
  if (!field || !shouldReplaceMidiLabel(field.value, ports)) return false;
  const preferred = preferredMidiPortName(ports);
  if (!preferred) return false;
  field.value = preferred;
  saveConfig(formValues());
  return true;
}

function appendMidiPortList(target, ports, emptyText) {
  if (!target) return;
  target.textContent = '';
  if (!Array.isArray(ports) || !ports.length) {
    const item = document.createElement('li');
    item.textContent = emptyText;
    target.appendChild(item);
    return;
  }
  ports.forEach((port) => {
    const item = document.createElement('li');
    item.textContent = describeMidiPort(port);
    target.appendChild(item);
  });
}

function renderMidiPorts({ inputs = [], outputs = [] } = {}) {
  latestMidiPorts = {
    inputs: Array.isArray(inputs) ? inputs : [],
    outputs: Array.isArray(outputs) ? outputs : [],
  };
  appendMidiPortList(
    midiInputsList,
    latestMidiPorts.inputs,
    'No MIDI inputs detected yet.',
  );
  appendMidiPortList(
    midiOutputsList,
    latestMidiPorts.outputs,
    'No MIDI outputs detected yet.',
  );
  midiPortsDatalist.textContent = '';
  const names = new Set();
  [...latestMidiPorts.inputs, ...latestMidiPorts.outputs].forEach((port) => {
    const name = midiPortName(port);
    if (name) names.add(name);
  });
  names.forEach((name) => {
    const option = document.createElement('option');
    option.value = name;
    midiPortsDatalist.appendChild(option);
  });
}

async function api(url, options = {}) {
  const { headers: optionHeaders = {}, ...requestOptions } = options;
  const response = await fetch(url, {
    headers: {
      'content-type': 'application/json',
      ...(controlToken ? { authorization: `Bearer ${controlToken}` } : {}),
      ...optionHeaders,
    },
    ...requestOptions,
  });
  const payload = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(
      payload?.error?.message ||
        payload.error ||
        `request failed: ${response.status}`,
    );
  }
  return payload;
}

function renderPreset(preset) {
  if (!preset) {
    recipeSummary.textContent = 'No preset selected.';
    recipeRequirements.textContent = '';
    recipeChecklist.textContent = '';
    return;
  }
  recipeSummary.textContent = [
    preset.host ? `${preset.host} recipe.` : '',
    ...(Array.isArray(preset.expectedTelemetry)
      ? preset.expectedTelemetry.slice(0, 1)
      : []),
  ]
    .filter(Boolean)
    .join(' ');
  recipeRequirements.textContent = '';
  recipeChecklist.textContent = '';
  (preset.requirements || []).forEach((text) => {
    const item = document.createElement('li');
    item.textContent = text;
    recipeRequirements.appendChild(item);
  });
  (preset.manualValidationChecklist || []).forEach((text) => {
    const item = document.createElement('li');
    item.textContent = text;
    recipeChecklist.appendChild(item);
  });
}

function applyPreset(preset) {
  if (!preset?.ports) return;
  populateForm({
    serialName:
      preset.ports.serialNameHint ||
      form.elements.namedItem('serialName')?.value,
    midiLabel: preset.ports.midiLabel,
    midiDestinationName: preset.host || preset.label || 'MIDI destination',
    oscHost: preset.ports.oscHost,
    oscPort: preset.ports.oscPort,
    oscDestinationName: preset.host || preset.label || 'OSC destination',
    oscListen: preset.ports.oscListen,
    oscBind: preset.ports.oscBind,
  });
  if (Array.isArray(preset.midiToOscMappings)) {
    consoleState.midiToOscMappings = clone(preset.midiToOscMappings);
    renderMappingList();
    renderMappingOutput(consoleState.bridge || {});
  }
  hostFormDirty = true;
  customSetupNotice = 'Bundled recipe loaded into the form; routing was not restarted.';
  renderCustomSetupStatus();
  saveConfig(formValues());
}

async function refreshPresets() {
  const payload = await api('/api/presets', { method: 'GET' });
  consoleState.presets = Array.isArray(payload.presets) ? payload.presets : [];
  presetSelect.textContent = '';
  consoleState.presets.forEach((entry, index) => {
    const option = document.createElement('option');
    option.value = entry.id;
    option.textContent = entry.label;
    if (!consoleState.activePresetId && index === 0) {
      consoleState.activePresetId = entry.id;
    }
    presetSelect.appendChild(option);
  });
  if (consoleState.activePresetId) {
    presetSelect.value = consoleState.activePresetId;
  }
  const selected = consoleState.presets.find(
    (entry) => entry.id === presetSelect.value,
  );
  renderPreset(selected?.preset);
  return consoleState.presets;
}

async function refreshState() {
  const payload = await api('/api/state', { method: 'GET' });
  if (!hostFormDirty) {
    populateForm(payload.state?.config);
    applyPreferredSerialPort();
    consoleState.midiToOscMappings = clone(
      payload.state?.config?.midiToOscMappings || [],
    );
    renderMappingList();
  }
  updateStatus(payload.state);
  return payload.state;
}

async function refreshPorts() {
  const payload = await api('/api/ports', { method: 'GET' });
  renderPorts(payload.ports);
  if (!hostFormDirty) applyPreferredSerialPort(payload.ports);
  return payload.ports;
}

async function refreshMidiPorts() {
  const payload = await api('/api/midi-ports', { method: 'GET' });
  renderMidiPorts(payload);
  if (!hostFormDirty) applyPreferredMidiPort(payload);
  return payload;
}

async function startBridge() {
  await refreshPorts().catch(() => {});
  await refreshMidiPorts().catch(() => {});
  const values = formValues();
  saveConfig(values);
  const payload = await api('/api/connect', {
    method: 'POST',
    body: JSON.stringify(values),
  });
  updateStatus(payload.state);
  populateForm(payload.state?.config);
  hostFormDirty = false;
  customSetupNotice = 'Current host form applied to the Bridge.';
  renderCustomSetupStatus();
}

async function stopBridge() {
  const payload = await api('/api/disconnect', {
    method: 'POST',
    body: JSON.stringify({}),
  });
  updateStatus(payload.state);
}

async function resetMetrics() {
  const payload = await api('/api/performance/reset', {
    method: 'POST',
    body: JSON.stringify({}),
  });
  updateStatus(payload.state);
}

async function clearAlerts() {
  const payload = await api('/api/alerts/clear', {
    method: 'POST',
    body: JSON.stringify({}),
  });
  updateStatus(payload.state);
}

async function downloadSnapshot() {
  const response = await fetch('/api/state/snapshot', { method: 'GET' });
  if (!response.ok) {
    throw new Error(`snapshot request failed: ${response.status}`);
  }
  const blob = await response.blob();
  const filenameHeader = response.headers.get('content-disposition') || '';
  const matched = filenameHeader.match(/filename="([^"]+)"/i);
  const filename = matched?.[1] || 'mn42-bridge-state.json';
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = filename;
  anchor.click();
  URL.revokeObjectURL(url);
}

function openConfigurator() {
  const target = new URL('/app/', window.location.href);
  target.searchParams.set('ws', wsUrl('/ws'));
  target.searchParams.set('bridgeTransport', 'session');
  if (controlToken) target.searchParams.set('token', controlToken);
  window.open(target.toString(), '_blank', 'noopener');
}

function connectRawSocket() {
  if (rawSocket && rawSocket.readyState <= 1) return;
  const target = new URL(wsUrl('/ws'));
  if (controlToken) target.searchParams.set('token', controlToken);
  rawSocket = new WebSocket(target.toString());
  rawSocket.addEventListener('message', (event) => {
    const text = String(event.data || '').trim();
    if (!text) return;
    pushLimited(consoleState.rawLines, text, RAW_LINE_LIMIT);
    observeSoundcheckTelemetry(text);
    renderRawSerialOutput();
  });
  rawSocket.addEventListener('close', () => {
    window.setTimeout(connectRawSocket, 1500);
  });
}

function connectStructuredSocket() {
  if (eventSocket && eventSocket.readyState <= 1) return;
  const target = new URL(wsUrl('/ws/events'));
  if (controlToken) target.searchParams.set('token', controlToken);
  eventSocket = new WebSocket(target.toString());
  eventSocket.addEventListener('message', (event) => {
    try {
      const payload = JSON.parse(String(event.data || '').trim());
      pushLimited(
        consoleState.structuredEvents,
        payload,
        STRUCTURED_EVENT_LIMIT,
      );
      if (payload?.event === 'route') {
        observeMappingLearnRoutes([payload.payload]);
      }
      renderStateJson(consoleState.bridge);
    } catch (_) {
      // ignore malformed structured frames in the UI
    }
  });
  eventSocket.addEventListener('close', () => {
    window.setTimeout(connectStructuredSocket, 1500);
  });
}

function bindEvents() {
  addMappingForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const mapping = mappingFormValue();
    if (
      !mapping.id ||
      !Number.isInteger(mapping.controller) ||
      mapping.controller < 0 ||
      mapping.controller > 127 ||
      !mapping.address.startsWith('/') ||
      mapping.address.startsWith('/mn42/')
    ) {
      mappingLearnStatus.textContent =
        'Enter an ID, a CC from 0–127, and a custom OSC address beginning with / but outside reserved /mn42/*.';
      return;
    }
    const preview = `${mapping.id}: MIDI Ch ${mapping.channel || 'any'} CC ${
      mapping.controller
    } → ${mapping.address} (${mapping.valueMode})`;
    if (
      !window.confirm(
        `Add this live mapping without restarting routing?\n\n${preview}`,
      )
    ) {
      return;
    }
    try {
      await commitMappings([...consoleState.midiToOscMappings, mapping]);
      addMappingForm.reset();
      renderMappingPreview();
      mappingLearnStatus.textContent =
        'Mapping added and active. Routing was not restarted.';
      summaryStatus.textContent = 'MIDI-to-OSC mapping added live.';
    } catch (error) {
      mappingLearnStatus.textContent = `Mapping add failed: ${error.message}`;
    }
  });

  startButton.addEventListener('click', async () => {
    summaryStatus.textContent = 'Starting bridge...';
    try {
      await startBridge();
    } catch (error) {
      summaryStatus.textContent = `Bridge start failed: ${error.message}`;
    }
  });

  stopButton.addEventListener('click', async () => {
    if (!window.confirm(operatorConfirmationMessage('stop'))) return;
    summaryStatus.textContent = 'Stopping bridge...';
    try {
      await stopBridge();
    } catch (error) {
      summaryStatus.textContent = `Bridge stop failed: ${error.message}`;
    }
  });

  refreshPortsButton.addEventListener('click', async () => {
    summaryStatus.textContent = 'Refreshing host inventory...';
    try {
      await refreshPorts();
      await refreshMidiPorts();
      await refreshState();
      summaryStatus.textContent = 'Host inventory refreshed.';
    } catch (error) {
      summaryStatus.textContent = `Port refresh failed: ${error.message}`;
    }
  });

  resetMetricsButton.addEventListener('click', async () => {
    try {
      await resetMetrics();
      summaryStatus.textContent = 'Performance metrics reset.';
    } catch (error) {
      summaryStatus.textContent = `Metrics reset failed: ${error.message}`;
    }
  });

  clearAlertsButton.addEventListener('click', async () => {
    if (!window.confirm(operatorConfirmationMessage('clearAlerts'))) return;
    try {
      await clearAlerts();
      summaryStatus.textContent = 'Active alerts cleared.';
    } catch (error) {
      summaryStatus.textContent = `Clear alerts failed: ${error.message}`;
    }
  });

  downloadSnapshotButton.addEventListener('click', async () => {
    try {
      await downloadSnapshot();
      summaryStatus.textContent = 'Snapshot downloaded.';
    } catch (error) {
      summaryStatus.textContent = `Snapshot failed: ${error.message}`;
    }
  });

  stageDownloadSnapshotButton?.addEventListener('click', () => {
    downloadSnapshot().catch(() => {});
  });
  stageRefreshStateButton?.addEventListener('click', () => {
    refreshState().catch(() => {});
  });
  openConfiguratorButton.addEventListener('click', openConfigurator);
  stageOpenConfiguratorButton?.addEventListener('click', openConfigurator);
  startSoundcheckButton?.addEventListener('click', startPassiveSoundcheck);
  startMappingLearnButton?.addEventListener('click', startMappingLearn);

  recentOscAddressSelect?.addEventListener('change', () => {
    if (!recentOscAddressSelect.value) return;
    addMappingForm.elements.namedItem('address').value =
      recentOscAddressSelect.value;
    renderMappingPreview();
  });

  addMappingForm.addEventListener('input', renderMappingPreview);

  form.addEventListener('input', () => {
    hostFormDirty = true;
    consoleState.loadedCustomSetupId = '';
    customSetupNotice = '';
    saveConfig(formValues());
    renderCustomSetupStatus();
  });

  outboundMidiMappingsInput?.addEventListener('input', () => {
    try {
      const parsed = JSON.parse(outboundMidiMappingsInput.value || '[]');
      const normalized = normalizeHostSetupConfig({
        ...formValues(),
        outboundMidiMappings: parsed,
      });
      consoleState.outboundMidiMappings = normalized?.outboundMidiMappings || [];
      outboundMidiMappingStatus.textContent = `${consoleState.outboundMidiMappings.length} valid outbound mapping${consoleState.outboundMidiMappings.length === 1 ? '' : 's'} staged.`;
      hostFormDirty = true;
      saveConfig(formValues());
    } catch (error) {
      outboundMidiMappingStatus.textContent = `Mapping JSON is not valid: ${error.message}`;
    }
  });
  midiTelemetryModeSelect?.addEventListener('change', () => {
    hostFormDirty = true;
    consoleState.loadedCustomSetupId = '';
    saveConfig(formValues());
    renderCustomSetupStatus();
  });

  [customSetupName, customSetupProfile, customSetupNotes].forEach((field) => {
    field?.addEventListener('input', () => {
      customSetupNotice = '';
      renderCustomSetupStatus();
    });
  });

  customSetupSelect?.addEventListener('change', () => {
    consoleState.activeCustomSetupId = customSetupSelect.value;
    customSetupNotice = selectedCustomSetup()
      ? 'Selected only; choose Load selected to fill the form.'
      : 'Enter a name, then save the current host form.';
    if (selectedCustomSetup()) {
      customSetupName.value = selectedCustomSetup().name;
      customSetupProfile.value = selectedCustomSetup().suggestedDeviceProfile || '';
      customSetupNotes.value = selectedCustomSetup().notes || '';
    }
    renderCustomSetupStatus();
  });
  newCustomSetupButton?.addEventListener('click', beginNewCustomSetup);
  saveCustomSetupButton?.addEventListener('click', saveCurrentCustomSetup);
  loadCustomSetupButton?.addEventListener('click', loadSelectedCustomSetup);
  deleteCustomSetupButton?.addEventListener('click', deleteSelectedCustomSetup);
  exportCustomSetupsButton?.addEventListener('click', exportCustomSetups);
  importCustomSetupsInput?.addEventListener('change', () => {
    const file = importCustomSetupsInput.files?.[0];
    if (file) importCustomSetups(file);
  });

  presetSelect?.addEventListener('change', () => {
    consoleState.loadedCustomSetupId = '';
    consoleState.activePresetId = presetSelect.value;
    const selected = consoleState.presets.find(
      (entry) => entry.id === presetSelect.value,
    );
    renderPreset(selected?.preset);
    applyPreset(selected?.preset);
  });

  modeTabs.forEach((button) => {
    button.addEventListener('click', () => updateMode(button.dataset.mode));
  });
}

async function boot() {
  consoleState.customSetups = loadCustomSetups();
  renderCustomSetups();
  populateForm(loadSavedConfig());
  updateMode(loadMode());
  bindEvents();
  await refreshPorts().catch((error) => {
    summaryStatus.textContent = `Port refresh failed: ${error.message}`;
  });
  await refreshMidiPorts().catch(() => {});
  await refreshPresets().catch(() => {});
  await refreshState().catch((error) => {
    summaryStatus.textContent = `State refresh failed: ${error.message}`;
  });
  connectRawSocket();
  connectStructuredSocket();
  window.setInterval(() => {
    refreshState().catch(() => {});
  }, 1500);
}

boot();
