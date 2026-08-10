/* eslint-env browser */

const STORAGE_KEY = 'mn42-bridge-console';
const MODE_KEY = 'mn42-bridge-console-mode';
const RAW_LINE_LIMIT = 200;
const STRUCTURED_EVENT_LIMIT = 120;
const controlToken = new URL(window.location.href).searchParams.get('token') || '';
const {
  activeAlerts: selectActiveAlerts,
  describeAuthority,
  describeConfigValidation,
  describeDraft,
  formatTelemetryFreshness,
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

const modeTabs = [...document.querySelectorAll('.mode-tab')];
const modeViews = [...document.querySelectorAll('[data-mode-view]')];

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
};
window.__MN42_BRIDGE_CONSOLE_STATE = consoleState;

let latestPorts = [];
let latestMidiPorts = { inputs: [], outputs: [] };
let rawSocket = null;
let eventSocket = null;

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
    oscHost: String(data.get('oscHost') || '').trim(),
    oscPort: Number(data.get('oscPort') || 9000),
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
  const tail = [route.traceId, route.source, route.destination]
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
}

function renderMappingOutput(state = {}) {
  if (!mappingOutput) return;
  const config = state.config || {};
  mappingOutput.textContent = formatJson({
    feedbackWindowMs: config.feedbackWindowMs ?? null,
    allowFeedbackLoops: Boolean(config.allowFeedbackLoops),
    midiLabel: config.midiLabel ?? null,
    midiToOscMappings: consoleState.midiToOscMappings || [],
  });
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
    row.innerHTML = `
      <td>${mapping.id || `mapping-${index + 1}`}</td>
      <td>CC ${mapping.controller}${
        mapping.channel ? ` (Ch ${mapping.channel})` : ''
      }</td>
      <td>${mapping.address}</td>
      <td>${mapping.valueMode}</td>
      <td class="actions">
        <button class="remove-mapping" data-index="${index}" type="button">Remove</button>
      </td>
    `;
    mappingListBody.appendChild(row);
  });

  mappingListBody.querySelectorAll('.remove-mapping').forEach((btn) => {
    btn.addEventListener('click', () => {
      const index = parseInt(btn.dataset.index, 10);
      consoleState.midiToOscMappings.splice(index, 1);
      renderMappingList();
      renderMappingOutput(consoleState.bridge || {});
      saveConfig(formValues());
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
    oscHost: preset.ports.oscHost,
    oscPort: preset.ports.oscPort,
    oscListen: preset.ports.oscListen,
    oscBind: preset.ports.oscBind,
  });
  if (Array.isArray(preset.midiToOscMappings)) {
    consoleState.midiToOscMappings = clone(preset.midiToOscMappings);
    renderMappingList();
    renderMappingOutput(consoleState.bridge || {});
  }
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
  populateForm(payload.state?.config);
  applyPreferredSerialPort();
  updateStatus(payload.state);
  consoleState.midiToOscMappings = clone(
    payload.state?.config?.midiToOscMappings || [],
  );
  renderMappingList();
  return payload.state;
}

async function refreshPorts() {
  const payload = await api('/api/ports', { method: 'GET' });
  renderPorts(payload.ports);
  applyPreferredSerialPort(payload.ports);
  return payload.ports;
}

async function refreshMidiPorts() {
  const payload = await api('/api/midi-ports', { method: 'GET' });
  renderMidiPorts(payload);
  applyPreferredMidiPort(payload);
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
  addMappingForm.addEventListener('submit', (e) => {
    e.preventDefault();
    const data = new FormData(addMappingForm);
    const mapping = {
      id: String(data.get('id') || '').trim(),
      controller: parseInt(data.get('controller'), 10),
      channel: data.get('channel') ? parseInt(data.get('channel'), 10) : null,
      address: String(data.get('address') || '').trim(),
      valueMode: data.get('valueMode') || 'raw',
    };
    if (!mapping.id || Number.isNaN(mapping.controller) || !mapping.address) {
      return;
    }
    consoleState.midiToOscMappings.push(mapping);
    renderMappingList();
    renderMappingOutput(consoleState.bridge || {});
    saveConfig(formValues());
    addMappingForm.reset();
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

  form.addEventListener('input', () => {
    saveConfig(formValues());
  });

  presetSelect?.addEventListener('change', () => {
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
