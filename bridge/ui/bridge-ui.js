/* eslint-env browser */

const STORAGE_KEY = 'mn42-bridge-console';

const form = document.getElementById('bridge-form');
const startButton = document.getElementById('start-bridge');
const stopButton = document.getElementById('stop-bridge');
const refreshPortsButton = document.getElementById('refresh-ports');
const openConfiguratorButton = document.getElementById('open-configurator');
const summaryStatus = document.getElementById('summary-status');
const portsList = document.getElementById('ports');
const portsDatalist = document.getElementById('serial-port-list');
const logOutput = document.getElementById('logs');

const statusNodes = {
  running: document.getElementById('status-running'),
  serial: document.getElementById('status-serial'),
  ready: document.getElementById('status-ready'),
  osc: document.getElementById('status-osc'),
  oscListen: document.getElementById('status-osc-listen'),
  midi: document.getElementById('status-midi'),
  telemetry: document.getElementById('status-telemetry'),
  error: document.getElementById('status-error'),
};

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

function formValues() {
  const data = new FormData(form);
  return {
    serialName: String(data.get('serialName') || '').trim(),
    midiLabel: String(data.get('midiLabel') || '').trim(),
    oscHost: String(data.get('oscHost') || '').trim(),
    oscPort: Number(data.get('oscPort') || 9000),
    oscListen: Number(data.get('oscListen') || 9000),
    oscBind: String(data.get('oscBind') || '').trim(),
  };
}

function populateForm(values) {
  if (!values || typeof values !== 'object') return;
  for (const [key, value] of Object.entries(values)) {
    const field = form.elements.namedItem(key);
    if (!field || value === undefined || value === null) continue;
    field.value = String(value);
  }
}

function formatWhen(isoString) {
  if (!isoString) return 'none yet';
  const date = new Date(isoString);
  if (Number.isNaN(date.getTime())) return String(isoString);
  return date.toLocaleString();
}

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

function wsUrl() {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  return `${protocol}//${window.location.host}/ws`;
}

function updateButtons(state) {
  const running = Boolean(state?.running);
  startButton.disabled = running;
  stopButton.disabled = !running;
  openConfiguratorButton.disabled = !running;
}

function updateStatus(state) {
  const config = state?.config || {};
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
  statusNodes.error.textContent = state?.lastError || 'none';
  summaryStatus.textContent = state?.running
    ? state?.ready
      ? 'Bridge live and device handshake confirmed.'
      : 'Bridge running. Waiting for the board handshake.'
    : 'Bridge idle.';
  logOutput.textContent = logsToText(state?.logs);
  updateButtons(state);
}

function renderPorts(ports) {
  portsList.textContent = '';
  portsDatalist.textContent = '';
  if (!Array.isArray(ports) || !ports.length) {
    const item = document.createElement('li');
    item.textContent =
      'No serial ports detected yet. You can still type a port path manually.';
    portsList.appendChild(item);
    return;
  }
  ports.forEach((port) => {
    const option = document.createElement('option');
    option.value = port.path;
    portsDatalist.appendChild(option);

    const item = document.createElement('li');
    item.textContent = `${port.path}${
      port.manufacturer ? ` — ${port.manufacturer}` : ''
    }`;
    portsList.appendChild(item);
  });
}

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

async function refreshState() {
  const payload = await api('/api/state', { method: 'GET' });
  updateStatus(payload.state);
  populateForm(payload.state?.config);
  return payload.state;
}

async function refreshPorts() {
  const payload = await api('/api/ports', { method: 'GET' });
  renderPorts(payload.ports);
  return payload.ports;
}

async function startBridge() {
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

function openConfigurator() {
  const target = new URL('/app/', window.location.href);
  target.searchParams.set('ws', wsUrl());
  window.open(target.toString(), '_blank', 'noopener');
}

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

  openConfiguratorButton.addEventListener('click', openConfigurator);

  form.addEventListener('input', () => {
    saveConfig(formValues());
  });
}

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
