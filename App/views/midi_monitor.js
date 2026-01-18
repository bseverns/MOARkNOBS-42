const MIDI_LOG_LIMIT = 1000;
const LOG_VISIBLE_COUNT = 60;
const CLOCK_PULSES_PER_QUARTER = 24;

function formatBytes(bytes) {
  return bytes
    .map((value) => {
      const num = Number(value);
      if (!Number.isFinite(num)) return '--';
      return num.toString(16).padStart(2, '0').toUpperCase();
    })
    .join(' ');
}

function formatTimestamp(value) {
  const date = new Date(value);
  return `${date.getHours().toString().padStart(2, '0')}:${date.getMinutes()
    .toString()
    .padStart(2, '0')}:${date.getSeconds().toString().padStart(2, '0')}`;
}

function clampBpm(value) {
  const num = Number(value);
  if (!Number.isFinite(num)) return 120;
  return Math.min(220, Math.max(60, num));
}

export class MidiMonitor {
  constructor({ container } = {}) {
    this.container = container;
    if (!container) return;
    if (typeof window === 'undefined') return;

    this.toggleButton = container.querySelector('#midi-panel-toggle');
    this.enableButton = container.querySelector('#midi-enable');
    this.statusLabel = container.querySelector('#midi-status');
    this.outputSelect = container.querySelector('#midi-output-select');
    this.clockToggle = container.querySelector('#midi-clock-toggle');
    this.bpmInput = container.querySelector('#midi-clock-bpm');
    this.bpmValue = container.querySelector('#midi-clock-bpm-value');
    this.logBody = container.querySelector('#midi-log-body');

    this.logEntries = [];
    this.pendingRender = false;
    this.access = null;
    this.inputs = new Map();
    this.outputs = new Map();
    this.activeOutput = null;
    this.clockRunning = false;
    this.clockTimer = null;
    this.clockBpm = clampBpm(this.bpmInput?.value ?? 120);
    this.visibleCount = LOG_VISIBLE_COUNT;

    this.toggleButton?.addEventListener('click', () => this.togglePanel());
    this.enableButton?.addEventListener('click', () => this.initMidiAccess());
    this.clockToggle?.addEventListener('click', () => this.toggleClock());
    this.bpmInput?.addEventListener('input', () => this.updateBpm());
    this.outputSelect?.addEventListener('change', () => this.selectOutput(this.outputSelect.value));

    this.setStatus('warn', 'Web MIDI inactive');
    this.updateClockLabel();
    this.updateClockButton();
  }

  togglePanel() {
    if (!this.container || !this.toggleButton) return;
    const expanded = !this.container.classList.toggle('collapsed');
    this.toggleButton.textContent = expanded ? 'Hide MIDI monitor' : 'Show MIDI monitor';
    this.toggleButton.setAttribute('aria-expanded', String(expanded));
  }

  async initMidiAccess() {
    if (this.access) return;
    if (!navigator.requestMIDIAccess) {
      this.setStatus('err', 'Web MIDI unsupported');
      return;
    }
    if (this.enableButton) this.enableButton.disabled = true;
    try {
      const access = await navigator.requestMIDIAccess({ sysex: false });
      this.access = access;
      this.access.onstatechange = () => this.refreshDevices();
      this.refreshDevices();
      this.setStatus('ok', 'MIDI access granted');
    } catch (err) {
      this.setStatus('err', `MIDI request failed: ${err?.message || err}`);
      if (this.enableButton) this.enableButton.disabled = false;
    }
  }

  refreshDevices() {
    if (!this.access) return;
    const inputs = Array.from(this.access.inputs?.values?.() ?? []);
    const outputs = Array.from(this.access.outputs?.values?.() ?? []);
    this.populateInputs(inputs);
    this.populateOutputs(outputs);
    if (outputs.length) {
      this.clockToggle?.removeAttribute('disabled');
    } else {
      this.clockToggle?.setAttribute('disabled', 'true');
    }
  }

  populateInputs(inputs) {
    inputs.forEach((input) => {
      if (!input || !input.id) return;
      if (this.inputs.has(input.id)) return;
      const handler = (event) => {
        const data = event?.data ? Array.from(event.data) : [];
        this.recordEntry('in', data, input.name ?? 'Input');
      };
      input.addEventListener('midimessage', handler);
      if (typeof input.onmidimessage === 'function') {
        input.onmidimessage = handler;
      }
      this.inputs.set(input.id, handler);
    });
  }

  populateOutputs(outputs) {
    this.outputs.clear();
    if (this.outputSelect) {
      this.outputSelect.innerHTML = '';
    }
    outputs.forEach((output) => {
      if (!output || !output.id) return;
      this.outputs.set(output.id, output);
      if (this.outputSelect) {
        const option = document.createElement('option');
        option.value = output.id;
        option.textContent = output.name || `Output ${output.id}`;
        this.outputSelect.appendChild(option);
      }
    });
    if (this.outputSelect) {
      const preferred = this.outputSelect.value || this.outputSelect.querySelector('option')?.value;
      if (preferred) {
        this.outputSelect.value = preferred;
        this.selectOutput(preferred);
      }
    }
  }

  selectOutput(id) {
    this.activeOutput = this.outputs.get(id) ?? null;
  }

  recordEntry(direction, bytes, label) {
    const safeBytes = Array.isArray(bytes) ? bytes : Array.from(bytes ?? []);
    this.logEntries.push({
      direction,
      bytes: safeBytes,
      label: label ?? '',
      timestamp: Date.now()
    });
    if (this.logEntries.length > MIDI_LOG_LIMIT) {
      this.logEntries.shift();
    }
    this.scheduleRender();
  }

  scheduleRender() {
    if (this.pendingRender) return;
    this.pendingRender = true;
    requestAnimationFrame(() => {
      this.pendingRender = false;
      this.renderLog();
    });
  }

  renderLog() {
    if (!this.logBody) return;
    const start = Math.max(0, this.logEntries.length - this.visibleCount);
    const entries = this.logEntries.slice(start);
    const fragment = document.createDocumentFragment();
    entries.forEach((entry) => {
      const row = document.createElement('div');
      row.className = 'midi-row';
      row.dataset.direction = entry.direction;
      const dir = document.createElement('span');
      dir.className = 'midi-row-direction';
      dir.textContent = entry.direction === 'out' ? 'OUT' : entry.direction === 'in' ? 'IN' : 'INFO';
      const bytes = document.createElement('span');
      bytes.className = 'midi-row-bytes';
      bytes.textContent = `${formatBytes(entry.bytes)}`;
      const time = document.createElement('span');
      time.className = 'midi-row-time';
      time.textContent = formatTimestamp(entry.timestamp);
      row.append(dir, bytes, time);
      fragment.appendChild(row);
    });
    this.logBody.textContent = '';
    this.logBody.appendChild(fragment);
    this.logBody.scrollTop = this.logBody.scrollHeight;
  }

  toggleClock() {
    if (this.clockRunning) {
      this.stopClock();
    } else {
      this.startClock();
    }
  }

  startClock() {
    if (!this.activeOutput) {
      this.setStatus('err', 'Select an output before starting the clock.');
      return;
    }
    this.clockRunning = true;
    this.updateClockButton();
    this.schedulePulse();
    this.setStatus('ok', `Clock running at ${this.clockBpm} BPM`);
  }

  stopClock() {
    this.clockRunning = false;
    this.updateClockButton();
    if (this.clockTimer) {
      clearTimeout(this.clockTimer);
      this.clockTimer = null;
    }
    this.setStatus('warn', 'Clock stopped');
  }

  schedulePulse() {
    if (!this.clockRunning) return;
    const interval = this.computeInterval();
    this.clockTimer = setTimeout(() => {
      this.emitClockPulse();
      this.schedulePulse();
    }, interval);
  }

  computeInterval() {
    return 60000 / (this.clockBpm * CLOCK_PULSES_PER_QUARTER);
  }

  emitClockPulse() {
    if (!this.activeOutput) {
      this.stopClock();
      return;
    }
    try {
      this.activeOutput.send([0xf8]);
      this.recordEntry('out', [0xf8], this.activeOutput.name ?? 'Clock');
    } catch (err) {
      this.setStatus('err', `Clock error: ${err?.message || err}`);
      this.stopClock();
    }
  }

  updateBpm() {
    if (!this.bpmInput) return;
    this.clockBpm = clampBpm(this.bpmInput.value);
    this.updateClockLabel();
    if (this.clockRunning) {
      this.stopClock();
      this.startClock();
    }
  }

  updateClockLabel() {
    if (this.bpmValue) {
      this.bpmValue.textContent = `${this.clockBpm}`;
    }
  }

  updateClockButton() {
    if (!this.clockToggle) return;
    if (this.clockRunning) {
      this.clockToggle.textContent = 'Stop clock';
    } else {
      this.clockToggle.textContent = 'Start clock';
    }
  }

  setStatus(level, message) {
    if (!this.statusLabel) return;
    this.statusLabel.textContent = message;
    this.statusLabel.dataset.level = level;
  }
}
