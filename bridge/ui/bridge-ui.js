/* eslint-env browser */

const STORAGE_KEY = 'mn42-bridge-console';

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
const logOutput = document.getElementById('logs');
const alertHistory = document.getElementById('alert-history');
let latestPorts = [];

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

// Restore the last bridge form values so the desktop helper reopens in a
// familiar state after refreshes or restarts.
function loadSavedConfig() {
  try {
    return JSON.parse(localStorage.getItem(STORAGE_KEY) || '{}');
  } catch (_) {
    return {};
  }
}

// Persist the current bridge form values locally; this is operator convenience,
// not shared bridge state.
function saveConfig(values) {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(values));
  } catch (_) {
    // no-op
  }
}

// Normalize the form into the JSON shape expected by the bridge HTTP API.
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
  };
}

// Fill the visible form from a stored config or the bridge's live state.
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
  if (
    label.includes('bluetooth') ||
    label.includes('wlan') ||
    label.includes('debug-console')
  ) {
    return 0;
  }
  return 10;
}

function displaySerialPath(path) {
  const value = String(path || '').trim();
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
  if (!Array.isArray(ports) || !ports.length) return false;
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

// Render ISO timestamps into operator-friendly local time labels.
function formatWhen(isoString) {
  if (!isoString) return 'none yet';
  const date = new Date(isoString);
  if (Number.isNaN(date.getTime())) return String(isoString);
  return date.toLocaleString();
}

function formatTimestampMs(raw) {
  if (raw === undefined || raw === null) return 'none yet';
  const asNum = Number(raw);
  if (!Number.isFinite(asNum)) return String(raw);
  return `${Math.trunc(asNum)} ms`;
}

function formatMetricMs(raw) {
  if (raw === undefined || raw === null) return 'n/a';
  const asNum = Number(raw);
  if (!Number.isFinite(asNum)) return 'n/a';
  return `${Math.trunc(asNum)} ms`;
}

// Flatten structured bridge logs into a simple scrolling text console.
function logsToText(logs) {
  if (!Array.isArray(logs) || !logs.length)
    return 'Waiting for bridge activity.';
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

// Point the configurator at this bridge's WebSocket endpoint.
function wsUrl() {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  return `${protocol}//${window.location.host}/ws`;
}

// Keep the primary controls aligned with whether the background bridge is live.
function updateButtons(state) {
  const running = Boolean(state?.running);
  startButton.disabled = running;
  stopButton.disabled = !running;
  openConfiguratorButton.disabled = !running;
}

function renderAlertHistory(recentAlerts = []) {
  if (!alertHistory) return;
  alertHistory.textContent = '';
  if (!Array.isArray(recentAlerts) || !recentAlerts.length) {
    const item = document.createElement('li');
    item.className = 'alerts-empty';
    item.textContent = 'No alerts yet.';
    alertHistory.appendChild(item);
    return;
  }
  const latestFirst = [...recentAlerts].slice(-12).reverse();
  for (const alert of latestFirst) {
    const item = document.createElement('li');
    const severity = alert?.severity || 'warn';
    item.className = `alert-item severity-${severity}`;
    const when = formatWhen(alert?.at);
    const code = alert?.code || 'alert';
    const message = alert?.message || code;
    item.textContent = `[${severity}] ${when} — ${message} (${code})`;
    alertHistory.appendChild(item);
  }
}

// Project the bridge daemon state into the dashboard summary and badges.
function updateStatus(state) {
  const config = state?.config || {};
  const counters = state?.counters || {};
  const timing = state?.timing || {};
  const roundTrip = state?.performance?.roundTrip || {};
  const performanceHealth = state?.performance?.health || {};
  const activeAlerts = Array.isArray(state?.alerts?.active)
    ? state.alerts.active
    : [];
  statusNodes.running.textContent = state?.running ? 'running' : 'stopped';
  statusNodes.serial.textContent = state?.serialConnected
    ? 'connected'
    : 'disconnected';
  statusNodes.ready.textContent = state?.ready ? 'ready' : 'awaiting HELLO';
  statusNodes.osc.textContent = `${config.oscHost || '127.0.0.1'}:${
    config.oscPort || 9000
  }`;
  statusNodes.oscListen.textContent = `${config.oscBind || '127.0.0.1'}:${
    config.oscListen || 9000
  }`;
  statusNodes.midi.textContent = config.midiLabel || 'MN42 Bridge';
  statusNodes.telemetry.textContent = formatWhen(state?.lastTelemetryAt);
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
  statusNodes.alertTop.classList.remove(
    'status-ok',
    'status-warn',
    'status-error',
  );
  if (activeAlerts.length) {
    statusNodes.alertTop.classList.add(
      activeAlerts[0].severity === 'error'
        ? 'status-error'
        : activeAlerts[0].severity === 'warn'
          ? 'status-warn'
          : 'status-ok',
    );
  }
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
    ? state?.ready
      ? 'Bridge live and device handshake confirmed.'
      : 'Bridge running. Waiting for the board handshake.'
    : 'Bridge idle.';
  logOutput.textContent = logsToText(state?.logs);
  renderAlertHistory(state?.alerts?.recent);
  updateButtons(state);
}

// Render the currently detected serial ports into both the visible list and the
// datalist used for typed path completion.
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

// Small JSON fetch helper shared by every control path in this UI.
async function api(url, options = {}) {
  const response = await fetch(url, {
    headers: { 'content-type': 'application/json' },
    ...options,
  });
  const payload = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(payload.error || `request failed: ${response.status}`);
  }
  return payload;
}

// Pull the bridge daemon's current status and repaint the dashboard.
async function refreshState() {
  const payload = await api('/api/state', { method: 'GET' });
  updateStatus(payload.state);
  populateForm(payload.state?.config);
  applyPreferredSerialPort();
  return payload.state;
}

// Refresh the serial-port inventory shown to the operator.
async function refreshPorts() {
  const payload = await api('/api/ports', { method: 'GET' });
  renderPorts(payload.ports);
  applyPreferredSerialPort(payload.ports);
  return payload.ports;
}

// Start the bridge with the current form values.
async function startBridge() {
  await refreshPorts().catch(() => {});
  applyPreferredSerialPort();
  const values = formValues();
  saveConfig(values);
  const payload = await api('/api/connect', {
    method: 'POST',
    body: JSON.stringify(values),
  });
  updateStatus(payload.state);
}

// Stop the running bridge process and repaint the dashboard state.
async function stopBridge() {
  const payload = await api('/api/disconnect', {
    method: 'POST',
    body: JSON.stringify({}),
  });
  updateStatus(payload.state);
}

// Reset rolling latency/jitter measurements without disconnecting the bridge.
async function resetMetrics() {
  const payload = await api('/api/performance/reset', {
    method: 'POST',
    body: JSON.stringify({}),
  });
  updateStatus(payload.state);
}

// Clear currently active alerts while preserving the recent alert history.
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

// Launch the browser configurator already pointed at this bridge instance.
function openConfigurator() {
  const target = new URL('/app/', window.location.href);
  target.searchParams.set('ws', wsUrl());
  window.open(target.toString(), '_blank', 'noopener');
}

// Attach button and form handlers once during page boot.
function bindEvents() {
  startButton.addEventListener('click', async () => {
    summaryStatus.textContent = 'Starting bridge...';
    try {
      await startBridge();
    } catch (err) {
      summaryStatus.textContent = `Bridge start failed: ${err.message}`;
    }
  });

  stopButton.addEventListener('click', async () => {
    summaryStatus.textContent = 'Stopping bridge...';
    try {
      await stopBridge();
    } catch (err) {
      summaryStatus.textContent = `Bridge stop failed: ${err.message}`;
    }
  });

  refreshPortsButton.addEventListener('click', async () => {
    summaryStatus.textContent = 'Refreshing port list...';
    try {
      await refreshPorts();
      await refreshState();
    } catch (err) {
      summaryStatus.textContent = `Port refresh failed: ${err.message}`;
    }
  });

  resetMetricsButton.addEventListener('click', async () => {
    summaryStatus.textContent = 'Resetting performance metrics...';
    try {
      await resetMetrics();
      summaryStatus.textContent = 'Performance metrics reset.';
    } catch (err) {
      summaryStatus.textContent = `Metrics reset failed: ${err.message}`;
    }
  });

  clearAlertsButton.addEventListener('click', async () => {
    summaryStatus.textContent = 'Clearing active alerts...';
    try {
      await clearAlerts();
      summaryStatus.textContent = 'Active alerts cleared.';
    } catch (err) {
      summaryStatus.textContent = `Clear alerts failed: ${err.message}`;
    }
  });

  downloadSnapshotButton.addEventListener('click', async () => {
    summaryStatus.textContent = 'Preparing bridge snapshot...';
    try {
      await downloadSnapshot();
      summaryStatus.textContent = 'Snapshot downloaded.';
    } catch (err) {
      summaryStatus.textContent = `Snapshot failed: ${err.message}`;
    }
  });

  openConfiguratorButton.addEventListener('click', openConfigurator);

  form.addEventListener('input', () => {
    saveConfig(formValues());
  });
}

// One-shot page bootstrap: restore saved values, wire events, fetch initial
// state, then keep polling the daemon summary.
async function boot() {
  populateForm(loadSavedConfig());
  bindEvents();
  await refreshPorts().catch((err) => {
    summaryStatus.textContent = `Port refresh failed: ${err.message}`;
  });
  await refreshState().catch((err) => {
    summaryStatus.textContent = `State refresh failed: ${err.message}`;
  });
  window.setInterval(() => {
    refreshState().catch(() => {});
  }, 1500);
}

boot();
