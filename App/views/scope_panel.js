import { modulationIdentity } from './modulation_identity.js';

// Tiny oscilloscope for firmware telemetry. The device sends EF values in
// 0..127 space and normalized LFO values; this file keeps enough history to
// make every modulation lane's movement visible.
const DEFAULT_EF_COUNT = 6;
const DEFAULT_LFO_COUNT = 2;
const MIN_HISTORY = 64;
const TARGET_FPS = 30;
const PEAK_DECAY = 0.003;
const DEFAULT_ACTIVITY_HOLD_MS = 1500;
const DEFAULT_TIME_WINDOW_SECONDS = 5;
const TIME_WINDOW_OPTIONS = Object.freeze([2, 5, 10]);

// Scope math stays in normalized 0..1 space regardless of the source lane.
const clamp01 = (value) => {
  const num = Number(value);
  if (!Number.isFinite(num)) return 0;
  return Math.min(1, Math.max(0, num));
};

// Shared time source for animation and FPS bookkeeping.
const now = () =>
  typeof performance === 'object' && typeof performance.now === 'function'
    ? performance.now()
    : Date.now();

// Canvas painter, not a data owner. Runtime pushes frames; the panel stores the
// last screenful and redraws at a boring fixed cadence.
export class ScopePanel {
  constructor({
    container,
    runtime,
    manifest,
    renderToggle = null,
    activityHoldMs = DEFAULT_ACTIVITY_HOLD_MS,
    timeWindowSeconds = DEFAULT_TIME_WINDOW_SECONDS,
    nowFn = now
  } = {}) {
    this.container = container;
    if (!container || typeof window === 'undefined') return;

    const findElement = (role, fallbackId) =>
      container.querySelector(`[data-scope-role="${role}"]`) ??
      (fallbackId ? container.querySelector(`#${fallbackId}`) : null);
    this.canvas = findElement('canvas', 'scope-canvas');
    this.statusLabel = findElement('status', 'scope-status');
    this.fpsLabel = findElement('fps', 'scope-fps');
    this.refreshButton = findElement('refresh', 'scope-refresh');
    this.snapshotButton = findElement('snapshot', 'scope-snapshot');
    this.efLegend = findElement('ef-legend', 'scope-ef-legend');
    this.viewStateLabel = findElement('view-state');
    this.leaveSoloButton = findElement('leave-solo');
    this.viewModeButtons = Array.from(container.querySelectorAll('[data-scope-view]'));
    this.timeWindowButtons = Array.from(container.querySelectorAll('[data-scope-window]'));
    const roleLfoLabels = Array.from(container.querySelectorAll('[data-scope-lfo-index]')).sort(
      (left, right) => Number(left.dataset.scopeLfoIndex) - Number(right.dataset.scopeLfoIndex)
    );
    this.lfoValueLabels = roleLfoLabels.length
      ? roleLfoLabels
      : [container.querySelector('#scope-lfo-1'), container.querySelector('#scope-lfo-2')];
    this.clockLabel = findElement('clock', 'scope-clock');
    this.renderToggle = renderToggle;
    this.stageSummary = renderToggle?.querySelector?.('[data-scope-summary]') ?? null;
    if (!this.canvas || !this.canvas.getContext) return;

    this.ctx = this.canvas.getContext('2d');
    this.runtime = runtime;
    this.activityHoldMs = Math.max(0, Number(activityHoldMs) || 0);
    this.timeWindowSeconds = TIME_WINDOW_OPTIONS.includes(Number(timeWindowSeconds))
      ? Number(timeWindowSeconds)
      : DEFAULT_TIME_WINDOW_SECONDS;
    this.now = typeof nowFn === 'function' ? nowFn : now;
    this.historyLength = Math.max(MIN_HISTORY, Math.round(this.canvas.width || MIN_HISTORY));
    this.efCount = DEFAULT_EF_COUNT;
    this.efHistory = Array.from(
      { length: this.efCount },
      () => new Float32Array(this.historyLength)
    );
    this.lfoCount = DEFAULT_LFO_COUNT;
    this.lfoHistory = Array.from(
      { length: this.lfoCount },
      () => new Float32Array(this.historyLength)
    );
    this.timestampHistory = new Float64Array(this.historyLength);
    this.cursor = 0;
    this.samples = 0;
    this.peakLevel = 0;
    this.lastEfValues = Array.from({ length: this.efCount }, () => 0);
    this.lastEfActive = Array.from({ length: this.efCount }, () => false);
    this.efLastActiveAt = Array.from({ length: this.efCount }, () => null);
    this.hasEfStatus = false;
    this.viewMode = 'active';
    this.soloEfIndex = null;
    this.hasTelemetry = false;
    this.lastTelemetryTimestamp = null;
    this.lastLfoValues = Array.from({ length: this.lfoCount }, () => 0);
    this.lastLfoConfigs = Array.from({ length: this.lfoCount }, () => null);
    this.lastClock = null;
    this.frameRequest = null;
    this.lastRender = 0;
    this.fpsWindowStart = this.now();
    this.fpsFrameCount = 0;

    this.refreshButton?.addEventListener('click', () => this.refreshScope());
    this.snapshotButton?.addEventListener('click', () => this.captureSnapshot());
    this.leaveSoloButton?.addEventListener('click', () => this.leaveSolo());
    this.viewModeButtons.forEach((button) => {
      button.addEventListener('click', () => this.setViewMode(button.dataset.scopeView));
    });
    this.timeWindowButtons.forEach((button) => {
      button.addEventListener('click', () => this.setTimeWindow(button.dataset.scopeWindow));
    });
    this.syncTimeWindowControls();
    this.handleRenderToggle = () => {
      if (this.renderToggle && !this.renderToggle.open) {
        this.stopRenderLoop();
        return;
      }
      this.resizeCanvas();
      this.draw();
      this.startRenderLoop();
    };
    this.renderToggle?.addEventListener('toggle', this.handleRenderToggle);

    this.telemetrySubscription = runtime?.on('telemetry', (frame) => this.handleTelemetry(frame));
    this.manifestSubscription = runtime?.on('manifest', (payload) => this.applyManifest(payload));
    this.applyManifest(runtime?.getState?.()?.manifest ?? manifest ?? {});

    this.resizeObserver =
      typeof ResizeObserver === 'function'
        ? new ResizeObserver(() => {
            this.resizeCanvas();
          })
        : null;
    this.resizeObserver?.observe(this.container);

    this.resizeCanvas();
    this.handleRenderToggle();
  }

  // Resize history buffers to match the manifest's advertised modulation lanes.
  applyManifest(manifest = {}) {
    const efCountCandidate = Number(manifest?.envelope_count);
    const efCount = Number.isFinite(efCountCandidate)
      ? Math.max(0, Math.floor(efCountCandidate))
      : DEFAULT_EF_COUNT;
    const lfoCountCandidate = Number(manifest?.lfo_count);
    const lfoCount = Number.isFinite(lfoCountCandidate)
      ? Math.max(DEFAULT_LFO_COUNT, Math.floor(lfoCountCandidate))
      : DEFAULT_LFO_COUNT;
    if (efCount === this.efCount && lfoCount === this.lfoCount) {
      this.renderEfLegend();
      this.syncViewControls();
      return;
    }
    this.efCount = efCount;
    this.lfoCount = lfoCount;
    if (this.soloEfIndex !== null && this.soloEfIndex >= this.efCount) {
      this.soloEfIndex = null;
    }
    this.initializeBuffers();
    this.renderEfLegend();
    this.syncViewControls();
  }

  // Rebuild all rolling buffers after a scope-size or manifest change.
  initializeBuffers() {
    this.efHistory = Array.from(
      { length: this.efCount },
      () => new Float32Array(this.historyLength)
    );
    this.lfoHistory = Array.from(
      { length: this.lfoCount },
      () => new Float32Array(this.historyLength)
    );
    this.timestampHistory = new Float64Array(this.historyLength);
    this.lastEfValues = Array.from({ length: this.efCount }, () => 0);
    this.lastEfActive = Array.from({ length: this.efCount }, () => false);
    this.efLastActiveAt = Array.from({ length: this.efCount }, () => null);
    this.hasEfStatus = false;
    this.lastLfoValues = Array.from({ length: this.lfoCount }, () => 0);
    this.lastLfoConfigs = Array.from({ length: this.lfoCount }, () => null);
    this.cursor = 0;
    this.samples = 0;
    this.peakLevel = 0;
    this.syncEfLegend();
    this.syncScopeState();
  }

  // Clear the rolling traces without tearing down the live telemetry subscription.
  refreshScope() {
    this.initializeBuffers();
    this.hasTelemetry = false;
    this.lastTelemetryTimestamp = null;
    this.lastClock = null;
    if (this.statusLabel) {
      this.statusLabel.textContent = 'Waiting for telemetry…';
    }
    this.syncScopeState();
    this.updateReadouts();
    this.draw();
  }

  // Match the canvas backing store and history size to the rendered widget dimensions.
  resizeCanvas() {
    if (!this.canvas || !this.ctx) return;
    const rect = this.canvas.getBoundingClientRect();
    const measuredWidth = Math.round(rect.width);
    const measuredHeight = Math.round(rect.height);
    if (measuredWidth <= 0 || measuredHeight <= 0) return;
    const width = Math.max(MIN_HISTORY, measuredWidth);
    const height = Math.max(80, measuredHeight);
    const dpr = window.devicePixelRatio || 1;
    const pixelWidth = Math.round(width * dpr);
    const pixelHeight = Math.round(height * dpr);
    if (this.canvas.width !== pixelWidth || this.canvas.height !== pixelHeight) {
      this.canvas.width = pixelWidth;
      this.canvas.height = pixelHeight;
    }
    this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    this.renderWidth = width;
    this.renderHeight = height;
    const desiredHistory = Math.max(MIN_HISTORY, Math.round(width));
    if (desiredHistory !== this.historyLength) {
      this.resizeHistoryBuffers(desiredHistory);
    }
  }

  // Preserve the newest samples when a hidden or resized scope gets a new
  // backing width. Opening the Stage drawer therefore reveals its pre-open
  // context instead of starting with an empty graph.
  resizeHistoryBuffers(nextLength) {
    const desired = Math.max(MIN_HISTORY, Math.round(Number(nextLength) || MIN_HISTORY));
    const previousLength = this.historyLength;
    const previousCursor = this.cursor;
    const copyCount = Math.min(this.samples, desired);
    const copyBuffer = (source, BufferType = Float32Array) => {
      const target = new BufferType(desired);
      for (let idx = 0; idx < copyCount; idx += 1) {
        const sourceIndex =
          (previousCursor - copyCount + idx + previousLength) % previousLength;
        target[idx] = source?.[sourceIndex] ?? 0;
      }
      return target;
    };
    this.efHistory = this.efHistory.map((buffer) => copyBuffer(buffer));
    this.lfoHistory = this.lfoHistory.map((buffer) => copyBuffer(buffer));
    this.timestampHistory = copyBuffer(this.timestampHistory, Float64Array);
    this.historyLength = desired;
    this.samples = copyCount;
    this.cursor = copyCount % desired;
  }

  // Ingest one telemetry frame into the rolling EF/LFO history buffers.
  handleTelemetry(frame = {}) {
    if (!this.ctx) return;
    const receivedAt = this.now();
    const envelopes = Array.isArray(frame.envelopes) ? frame.envelopes : null;
    const lfos = Array.isArray(frame.lfos) ? frame.lfos : null;
    if (Array.isArray(frame.efStatus)) {
      this.hasEfStatus = true;
      frame.efStatus.forEach((value, idx) => {
        if (idx >= this.lastEfActive.length) return;
        const active = Boolean(Number(value));
        this.lastEfActive[idx] = active;
        if (active) this.efLastActiveAt[idx] = receivedAt;
      });
    }
    if (Array.isArray(frame.lfo_config)) {
      frame.lfo_config.forEach((entry, idx) => {
        const index = Number.isFinite(Number(entry?.index)) ? Number(entry.index) : idx;
        if (index >= 0 && index < this.lastLfoConfigs.length) {
          this.lastLfoConfigs[index] = entry;
        }
      });
    }
    this.efHistory.forEach((buffer, idx) => {
      const candidate = envelopes?.[idx];
      const hasEfValue = Number.isFinite(Number(candidate));
      const value = hasEfValue
        ? clamp01(Number(candidate) / 127)
        : this.lastEfValues[idx] ?? 0;
      buffer[this.cursor] = value;
      this.lastEfValues[idx] = value;
      this.peakLevel = Math.max(this.peakLevel, value);
    });
    this.lfoHistory.forEach((buffer, idx) => {
      const candidate = lfos?.[idx];
      const hasLfoValue = Number.isFinite(Number(candidate));
      const value = hasLfoValue ? clamp01(candidate) : this.lastLfoValues[idx] ?? 0;
      buffer[this.cursor] = value;
      this.lastLfoValues[idx] = value;
    });
    this.timestampHistory[this.cursor] = receivedAt;
    if (frame.clock && typeof frame.clock === 'object') {
      this.lastClock = frame.clock;
    }
    this.cursor = (this.cursor + 1) % this.historyLength;
    this.samples = Math.min(this.samples + 1, this.historyLength);
    this.hasTelemetry = true;
    this.lastTelemetryTimestamp = receivedAt;
    this.syncEfLegend();
    this.syncScopeState(receivedAt);
  }

  // Kick off the scope repaint loop once the panel is live.
  startRenderLoop() {
    if (this.frameRequest !== null) return;
    if (this.renderToggle && !this.renderToggle.open) return;
    this.container.dataset.scopeRendering = 'true';
    this.frameRequest = requestAnimationFrame((timestamp) => this.renderFrame(timestamp));
  }

  stopRenderLoop() {
    if (this.frameRequest !== null) cancelAnimationFrame(this.frameRequest);
    this.frameRequest = null;
    if (this.container) this.container.dataset.scopeRendering = 'false';
  }

  // Frame limiter that keeps the scope readable without hogging the browser.
  renderFrame(timestamp) {
    this.frameRequest = null;
    if (this.renderToggle && !this.renderToggle.open) {
      this.stopRenderLoop();
      return;
    }
    this.frameRequest = requestAnimationFrame((next) => this.renderFrame(next));
    if (timestamp - this.lastRender < 1000 / TARGET_FPS) return;
    this.lastRender = timestamp;
    this.draw();
    this.updateFps(timestamp);
  }

  destroy() {
    this.stopRenderLoop();
    this.telemetrySubscription?.();
    this.manifestSubscription?.();
    this.resizeObserver?.disconnect?.();
    this.renderToggle?.removeEventListener('toggle', this.handleRenderToggle);
  }

  // Draw the background grid, selected EF traces, LFO traces, and peak marker.
  draw() {
    if (!this.ctx || !this.renderWidth || !this.renderHeight) return;
    const ctx = this.ctx;
    const w = this.renderWidth;
    const h = this.renderHeight;
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = 'rgba(8, 12, 19, 0.6)';
    ctx.fillRect(0, 0, w, h);
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
    ctx.lineWidth = 1;
    [0.25, 0.5, 0.75].forEach((factor) => {
      const y = h - factor * h;
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(w, y);
      ctx.stroke();
    });
    [0.25, 0.5, 0.75].forEach((factor) => {
      const x = factor * w;
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
      ctx.stroke();
    });

    const timestamp = this.now();
    const visibleEfIndices = this.visibleEfIndices(timestamp);
    visibleEfIndices.forEach((idx) => {
      const identity = modulationIdentity('ef', idx);
      this.drawLine(
        ctx,
        this.efHistory[idx],
        identity.rgba(this.efTraceOpacity(idx, timestamp)),
        w,
        h,
        { lineWidth: 2.2, timestamp }
      );
    });
    this.lfoHistory.forEach((buffer, idx) => {
      const identity = modulationIdentity('lfo', idx);
      this.drawLine(ctx, buffer, identity.rgba(0.7), w, h, {
        lineWidth: 1.35,
        lineDash: [6, 4],
        timestamp
      });
    });
    if (
      this.hasTelemetry &&
      this.hasEfStatus &&
      this.viewMode === 'active' &&
      this.soloEfIndex === null &&
      visibleEfIndices.length === 0
    ) {
      this.drawNoActiveEfState(ctx, w, h);
    }

    if (this.peekPeakLine(w, h)) {
      ctx.strokeStyle = 'rgba(255, 255, 255, 0.8)';
      ctx.lineWidth = 2;
      const y = h - this.peakLevel * h;
      ctx.beginPath();
      ctx.moveTo(w - 30, y);
      ctx.lineTo(w, y);
      ctx.stroke();
      this.peakLevel = Math.max(0, this.peakLevel - PEAK_DECAY);
    }
    this.drawTimeAxis(ctx, w, h);

    this.updateStatus();
    this.updateReadouts();
    this.syncEfLegend(timestamp);
    this.syncScopeState(timestamp);
  }

  // Stroke one normalized history buffer across the current canvas dimensions.
  drawLine(
    ctx,
    buffer,
    color,
    width,
    height,
    { lineWidth = 2, lineDash = [], timestamp = this.now() } = {}
  ) {
    if (!buffer) return;
    const sampleIndices = this.visibleSampleIndices(timestamp);
    if (sampleIndices.length < 2) return;
    const windowMs = this.timeWindowSeconds * 1000;
    ctx.strokeStyle = color;
    ctx.lineWidth = lineWidth;
    ctx.setLineDash(lineDash);
    ctx.beginPath();
    let started = false;
    for (const bufferIndex of sampleIndices) {
      const sampleTimestamp = this.timestampHistory[bufferIndex];
      const ageMs = Math.max(0, timestamp - sampleTimestamp);
      const value = clamp01(buffer[bufferIndex]);
      const x = width - (ageMs / windowMs) * width;
      const y = height - value * height;
      if (!started) {
        ctx.moveTo(x, y);
        started = true;
      } else {
        ctx.lineTo(x, y);
      }
    }
    ctx.stroke();
    ctx.setLineDash([]);
  }

  visibleSampleIndices(timestamp = this.now()) {
    const sampleCount = Math.min(this.samples, this.historyLength);
    const windowMs = this.timeWindowSeconds * 1000;
    const indices = [];
    for (let idx = 0; idx < sampleCount; idx += 1) {
      const bufferIndex =
        (this.cursor - sampleCount + idx + this.historyLength) % this.historyLength;
      const sampleTimestamp = this.timestampHistory[bufferIndex];
      if (!Number.isFinite(sampleTimestamp) || sampleTimestamp <= 0) continue;
      if (Math.max(0, timestamp - sampleTimestamp) <= windowMs) indices.push(bufferIndex);
    }
    return indices;
  }

  drawTimeAxis(ctx, width, height) {
    ctx.save();
    ctx.fillStyle = 'rgba(232, 238, 255, 0.5)';
    ctx.font = '10px ui-monospace, SFMono-Regular, Menlo, Consolas, monospace';
    ctx.textBaseline = 'bottom';
    ctx.textAlign = 'left';
    ctx.fillText(`-${this.timeWindowSeconds}s`, 7, height - 5);
    ctx.textAlign = 'right';
    ctx.fillText('now', width - 7, height - 5);
    ctx.restore();
  }

  // Decide whether the peak-hold marker should be shown this frame.
  peekPeakLine(width, height) {
    if (!this.hasTelemetry || this.peakLevel <= 0) return false;
    return true;
  }

  drawNoActiveEfState(ctx, width, height) {
    ctx.save();
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillStyle = 'rgba(8, 12, 19, 0.78)';
    ctx.fillRect(Math.max(12, width / 2 - 150), height / 2 - 30, Math.min(300, width - 24), 60);
    ctx.fillStyle = 'rgba(232, 238, 255, 0.82)';
    ctx.font = '600 12px ui-monospace, SFMono-Regular, Menlo, Consolas, monospace';
    ctx.fillText('Waiting for envelope activity', width / 2, height / 2 - 8);
    ctx.fillStyle = 'rgba(232, 238, 255, 0.56)';
    ctx.font = '11px ui-monospace, SFMono-Regular, Menlo, Consolas, monospace';
    ctx.fillText('EF histories are still being recorded.', width / 2, height / 2 + 12);
    ctx.restore();
  }

  efActivityState(index, timestamp = this.now()) {
    if (!this.hasEfStatus) return 'unknown';
    if (this.lastEfActive[index]) return 'active';
    const lastActiveAt = this.efLastActiveAt[index];
    if (
      Number.isFinite(lastActiveAt) &&
      Math.max(0, timestamp - lastActiveAt) < this.activityHoldMs
    ) {
      return 'recent';
    }
    return 'inactive';
  }

  efTraceOpacity(index, timestamp = this.now()) {
    if (this.soloEfIndex === index || !this.hasEfStatus) return 0.88;
    const state = this.efActivityState(index, timestamp);
    if (state === 'active') return 0.88;
    if (state === 'inactive') return 0.38;
    const elapsed = Math.max(0, timestamp - this.efLastActiveAt[index]);
    const remaining = this.activityHoldMs
      ? Math.max(0, 1 - elapsed / this.activityHoldMs)
      : 0;
    return 0.38 + remaining * 0.5;
  }

  visibleEfIndices(timestamp = this.now()) {
    if (this.soloEfIndex !== null) return [this.soloEfIndex];
    const all = Array.from({ length: this.efCount }, (_, idx) => idx);
    if (this.viewMode === 'all' || !this.hasEfStatus) return all;
    return all.filter((idx) => ['active', 'recent'].includes(this.efActivityState(idx, timestamp)));
  }

  setViewMode(mode) {
    if (!['active', 'all'].includes(mode)) return;
    this.viewMode = mode;
    this.soloEfIndex = null;
    this.syncViewControls();
    this.syncEfLegend();
    this.draw();
  }

  setTimeWindow(seconds) {
    const candidate = Number(seconds);
    if (!TIME_WINDOW_OPTIONS.includes(candidate)) return;
    this.timeWindowSeconds = candidate;
    this.syncTimeWindowControls();
    this.draw();
  }

  syncTimeWindowControls() {
    this.timeWindowButtons.forEach((button) => {
      const selected = Number(button.dataset.scopeWindow) === this.timeWindowSeconds;
      button.setAttribute('aria-pressed', selected ? 'true' : 'false');
    });
  }

  setSoloEf(index) {
    const candidate = Number(index);
    if (!Number.isInteger(candidate) || candidate < 0 || candidate >= this.efCount) return;
    this.soloEfIndex = this.soloEfIndex === candidate ? null : candidate;
    this.syncViewControls();
    this.syncEfLegend();
    this.draw();
  }

  leaveSolo() {
    if (this.soloEfIndex === null) return;
    this.soloEfIndex = null;
    this.syncViewControls();
    this.syncEfLegend();
    this.draw();
  }

  syncViewControls() {
    this.viewModeButtons.forEach((button) => {
      const selected = this.soloEfIndex === null && button.dataset.scopeView === this.viewMode;
      button.setAttribute('aria-pressed', selected ? 'true' : 'false');
    });
  }

  renderEfLegend() {
    if (!this.efLegend) return;
    this.efLegend.replaceChildren(
      ...Array.from({ length: this.efCount }, (_, idx) => {
        const identity = modulationIdentity('ef', idx);
        const button = document.createElement('button');
        button.type = 'button';
        button.className = 'scope-legend-item ef';
        button.dataset.efIndex = String(idx);
        button.style.setProperty('--modulation-color', identity.color);
        button.addEventListener('click', () => this.setSoloEf(idx));
        const label = document.createElement('span');
        label.textContent = identity.label;
        const value = document.createElement('b');
        value.textContent = '--';
        button.append(label, value);
        return button;
      })
    );
    this.syncEfLegend();
  }

  syncEfLegend(timestamp = this.now()) {
    if (!this.efLegend) return;
    this.efLegend.querySelectorAll('[data-ef-index]').forEach((button) => {
      const idx = Number(button.dataset.efIndex);
      const rawValue = Math.round(clamp01(this.lastEfValues[idx]) * 127);
      const state = this.efActivityState(idx, timestamp);
      const selected = this.soloEfIndex === idx;
      const valueLabel = button.querySelector('b');
      if (valueLabel) valueLabel.textContent = this.hasTelemetry ? String(rawValue) : '--';
      button.dataset.state = state;
      button.dataset.focus = selected ? 'soloed' : 'none';
      button.setAttribute('aria-pressed', selected ? 'true' : 'false');
      button.setAttribute(
        'aria-label',
        `EF ${idx + 1}, ${state}, ${this.hasTelemetry ? rawValue : 'no value'}; ${
          selected ? 'leave solo view' : 'solo this follower'
        }`
      );
      button.title = `EF ${idx + 1}: ${state}; click to ${selected ? 'leave solo' : 'solo'}`;
    });
  }

  syncScopeState(timestamp = this.now()) {
    const states = Array.from({ length: this.efCount }, (_, idx) =>
      this.efActivityState(idx, timestamp)
    );
    const activeCount = states.filter((state) => state === 'active').length;
    const recentCount = states.filter((state) => state === 'recent').length;
    const visibleCount = this.visibleEfIndices(timestamp).length;
    const lfoLabel = `${this.lfoCount} ${this.lfoCount === 1 ? 'LFO' : 'LFOs'}`;
    let viewText;

    if (!this.hasTelemetry) {
      viewText = `VIEW: ${this.viewMode.toUpperCase()} · Waiting for telemetry`;
    } else if (this.soloEfIndex !== null) {
      const rawValue = Math.round(clamp01(this.lastEfValues[this.soloEfIndex]) * 127);
      viewText = `VIEW: ${this.viewMode.toUpperCase()} · FOCUS: EF ${this.soloEfIndex + 1} · ${rawValue}`;
    } else if (!this.hasEfStatus) {
      viewText = `VIEW: ${this.viewMode.toUpperCase()} · EF activity unknown · ${lfoLabel} always visible`;
    } else if (this.viewMode === 'all') {
      viewText = `VIEW: ALL · ${this.efCount}/${this.efCount} EFs · ${lfoLabel} always visible`;
    } else if (recentCount) {
      viewText = `VIEW: ACTIVE · ${visibleCount}/${this.efCount} EFs (${activeCount} active, ${recentCount} recent) · ${lfoLabel} always visible`;
    } else if (activeCount) {
      viewText = `VIEW: ACTIVE · ${activeCount}/${this.efCount} EFs · ${lfoLabel} always visible`;
    } else {
      viewText = `VIEW: ACTIVE · No active EFs · ${lfoLabel} still running`;
    }

    if (this.viewStateLabel && this.viewStateLabel.textContent !== viewText) {
      this.viewStateLabel.textContent = viewText;
    }
    if (this.leaveSoloButton) this.leaveSoloButton.hidden = this.soloEfIndex === null;
    if (this.canvas) {
      const empty =
        this.hasTelemetry &&
        this.hasEfStatus &&
        this.viewMode === 'active' &&
        this.soloEfIndex === null &&
        visibleCount === 0;
      this.canvas.dataset.efEmpty = empty ? 'true' : 'false';
      this.canvas.setAttribute(
        'aria-label',
        empty
          ? `Envelope follower and LFO scope over ${this.timeWindowSeconds} seconds. No active envelope followers; histories are still recording and LFOs remain visible.`
          : `Envelope follower and LFO scope over ${this.timeWindowSeconds} seconds. ${viewText}`
      );
    }

    if (!this.stageSummary) return;
    let stageText;
    if (!this.hasTelemetry) {
      stageText = 'Waiting for modulation';
    } else if (this.soloEfIndex !== null) {
      const rawValue = Math.round(clamp01(this.lastEfValues[this.soloEfIndex]) * 127);
      stageText = `EF ${this.soloEfIndex + 1} solo · ${rawValue}`;
    } else if (this.viewMode === 'all') {
      stageText = `All ${this.efCount} EFs · ${lfoLabel}`;
    } else if (!this.hasEfStatus) {
      stageText = `EF activity pending · ${lfoLabel}`;
    } else if (activeCount && recentCount) {
      stageText = `${activeCount} active + ${recentCount} recent · ${lfoLabel}`;
    } else if (activeCount) {
      stageText = `${activeCount} ${activeCount === 1 ? 'EF' : 'EFs'} active · ${lfoLabel}`;
    } else if (recentCount) {
      stageText = `${recentCount} ${recentCount === 1 ? 'EF' : 'EFs'} recently active · ${lfoLabel}`;
    } else {
      stageText = `No active EFs · ${lfoLabel} still running`;
    }
    if (this.stageSummary.textContent !== stageText) this.stageSummary.textContent = stageText;
  }

  updateReadouts() {
    this.lfoValueLabels.forEach((label, idx) => {
      if (!label) return;
      const value = this.lastLfoValues[idx];
      this.updateLfoLegendLabel(label, idx, this.lastLfoConfigs[idx]);
      label.textContent = Number.isFinite(value) ? value.toFixed(2) : '--';
    });
    if (!this.clockLabel) return;
    const clock = this.lastClock;
    if (!clock) {
      this.clockLabel.textContent = 'Clock --';
      this.clockLabel.dataset.state = 'muted';
      return;
    }
    const source = clock.source ? String(clock.source) : 'idle';
    const running = Boolean(clock.running || clock.external_signal);
    this.clockLabel.textContent = `Clock ${source}`;
    this.clockLabel.dataset.state = running ? 'running' : 'idle';
  }

  formatLfoConfig(config, { compact = false } = {}) {
    if (!config || typeof config !== 'object') return '';
    const shape = String(config.shape_name || `S${Number(config.shape) || 0}`).replace(
      'Sample & Hold',
      compact ? 'S&H' : 'Sample & Hold'
    );
    const depth = Number(config.depth);
    const depthLabel = Number.isFinite(depth) ? `D${depth.toFixed(2).replace(/0$/, '')}` : '';
    const sync = Boolean(config.sync);
    const rate = sync
      ? String(config.sync_ratio_name || '')
      : Number.isFinite(Number(config.frequency_hz))
        ? `${Number(config.frequency_hz)
            .toFixed(2)
            .replace(/\.?0+$/, '')}Hz`
        : '';
    return [shape, depthLabel, rate].filter(Boolean).join(compact ? ' ' : ' · ');
  }

  updateLfoLegendLabel(valueLabel, idx, config) {
    const legend = valueLabel.closest?.('.scope-legend-item');
    if (!legend) return;
    const descriptor = this.formatLfoConfig(config, { compact: true });
    const prefix = descriptor ? `LFO ${idx + 1} ${descriptor} ` : `LFO ${idx + 1} `;
    const firstNode = Array.from(legend.childNodes).find(
      (node) => node.nodeType === Node.TEXT_NODE
    );
    if (firstNode) {
      firstNode.nodeValue = prefix;
    }
    legend.title = descriptor
      ? `LFO ${idx + 1}: ${this.formatLfoConfig(config)}`
      : `LFO ${idx + 1}`;
  }

  // Keep the scope status line truthful about streaming vs stale telemetry.
  updateStatus() {
    if (!this.statusLabel) return;
    if (!this.hasTelemetry) {
      this.statusLabel.textContent = 'Waiting for telemetry…';
      if (this.snapshotButton) this.snapshotButton.disabled = true;
      return;
    }
    if (this.snapshotButton) this.snapshotButton.disabled = false;
    const age = this.lastTelemetryTimestamp
      ? Math.max(0, Math.round(now() - this.lastTelemetryTimestamp))
      : 0;
    this.statusLabel.textContent = age ? `Telemetry ${age} ms ago` : 'Telemetry streaming…';
  }

  // Maintain a simple rolling FPS estimate for the panel footer.
  updateFps(timestamp) {
    this.fpsFrameCount += 1;
    const elapsed = timestamp - this.fpsWindowStart;
    if (elapsed >= 1000) {
      const fps = Math.round((this.fpsFrameCount * 1000) / (elapsed || 1));
      this.fpsFrameCount = 0;
      this.fpsWindowStart = timestamp;
      if (this.fpsLabel) this.fpsLabel.textContent = `${fps} fps`;
    }
  }

  // Export the current scope canvas as a PNG snapshot.
  captureSnapshot() {
    if (!this.canvas) return;
    if (typeof this.canvas.toBlob === 'function') {
      this.canvas.toBlob((blob) => {
        if (blob) {
          this.downloadBlob(blob);
        }
      }, 'image/png');
      return;
    }
    const dataUrl = this.canvas.toDataURL('image/png');
    this.downloadDataUrl(dataUrl);
  }

  // Download a blob-backed snapshot while avoiding URL leaks.
  downloadBlob(blob) {
    const name = `mn42-scope-${new Date().toISOString().replace(/[:.]/g, '-')}.png`;
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = name;
    anchor.click();
    setTimeout(() => {
      URL.revokeObjectURL(url);
    }, 200);
  }

  // Fallback download path for browsers that only expose data URLs.
  downloadDataUrl(dataUrl) {
    const name = `mn42-scope-${new Date().toISOString().replace(/[:.]/g, '-')}.png`;
    const anchor = document.createElement('a');
    anchor.href = dataUrl;
    anchor.download = name;
    anchor.click();
  }
}
