const DEFAULT_LFO_COUNT = 2;
const MIN_HISTORY = 64;
const TARGET_FPS = 30;
const PEAK_DECAY = 0.003;

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

export class ScopePanel {
  constructor({ container, runtime, manifest } = {}) {
    this.container = container;
    if (!container || typeof window === 'undefined') return;

    this.canvas = container.querySelector('#scope-canvas');
    this.statusLabel = container.querySelector('#scope-status');
    this.fpsLabel = container.querySelector('#scope-fps');
    this.snapshotButton = container.querySelector('#scope-snapshot');
    this.lfoValueLabels = [
      container.querySelector('#scope-lfo-1'),
      container.querySelector('#scope-lfo-2')
    ];
    this.clockLabel = container.querySelector('#scope-clock');
    if (!this.canvas || !this.canvas.getContext) return;

    this.ctx = this.canvas.getContext('2d');
    this.runtime = runtime;
    this.historyLength = Math.max(MIN_HISTORY, Math.round(this.canvas.width || MIN_HISTORY));
    this.efHistory = new Float32Array(this.historyLength);
    this.lfoCount = DEFAULT_LFO_COUNT;
    this.lfoHistory = Array.from(
      { length: this.lfoCount },
      () => new Float32Array(this.historyLength)
    );
    this.cursor = 0;
    this.samples = 0;
    this.peakLevel = 0;
    this.hasTelemetry = false;
    this.lastTelemetryTimestamp = null;
    this.lastLfoValues = Array.from({ length: this.lfoCount }, () => 0);
    this.lastLfoConfigs = Array.from({ length: this.lfoCount }, () => null);
    this.lastClock = null;
    this.frameRequest = null;
    this.lastRender = 0;
    this.fpsWindowStart = now();
    this.fpsFrameCount = 0;

    this.snapshotButton?.addEventListener('click', () => this.captureSnapshot());

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
    this.startRenderLoop();
  }

  // Resize LFO history buffers to match the manifest's advertised modulation lanes.
  applyManifest(manifest = {}) {
    const lfoCountCandidate = Number(manifest?.lfo_count);
    const lfoCount = Number.isFinite(lfoCountCandidate)
      ? Math.max(0, Math.floor(lfoCountCandidate))
      : DEFAULT_LFO_COUNT;
    if (lfoCount === this.lfoCount) return;
    this.lfoCount = Math.max(DEFAULT_LFO_COUNT, lfoCount);
    this.initializeBuffers();
  }

  // Rebuild all rolling buffers after a scope-size or manifest change.
  initializeBuffers() {
    this.efHistory = new Float32Array(this.historyLength);
    this.lfoHistory = Array.from(
      { length: this.lfoCount },
      () => new Float32Array(this.historyLength)
    );
    this.lastLfoValues = Array.from({ length: this.lfoCount }, () => 0);
    this.lastLfoConfigs = Array.from({ length: this.lfoCount }, () => null);
    this.cursor = 0;
    this.samples = 0;
    this.peakLevel = 0;
  }

  // Match the canvas backing store and history size to the rendered widget dimensions.
  resizeCanvas() {
    if (!this.canvas || !this.ctx) return;
    const rect = this.canvas.getBoundingClientRect();
    const width = Math.max(MIN_HISTORY, Math.round(rect.width));
    const height = Math.max(80, Math.round(rect.height));
    if (!width || !height) return;
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
      this.historyLength = desiredHistory;
      this.initializeBuffers();
    }
  }

  // Ingest one telemetry frame into the rolling EF/LFO history buffers.
  handleTelemetry(frame = {}) {
    if (!this.ctx) return;
    const envelopes = Array.isArray(frame.envelopes) ? frame.envelopes : [];
    const lfos = Array.isArray(frame.lfos) ? frame.lfos : null;
    if (Array.isArray(frame.lfo_config)) {
      frame.lfo_config.forEach((entry, idx) => {
        const index = Number.isFinite(Number(entry?.index)) ? Number(entry.index) : idx;
        if (index >= 0 && index < this.lastLfoConfigs.length) {
          this.lastLfoConfigs[index] = entry;
        }
      });
    }
    const aggregated = envelopes.length
      ? Math.max(...envelopes.map((value) => clamp01((Number(value) || 0) / 127)))
      : 0;
    this.efHistory[this.cursor] = aggregated;
    this.peakLevel = Math.max(this.peakLevel, aggregated);
    this.lfoHistory.forEach((buffer, idx) => {
      const candidate = lfos?.[idx];
      const hasLfoValue = Number.isFinite(Number(candidate));
      const value = hasLfoValue ? clamp01(candidate) : this.lastLfoValues[idx] ?? 0;
      buffer[this.cursor] = value;
      this.lastLfoValues[idx] = value;
    });
    if (frame.clock && typeof frame.clock === 'object') {
      this.lastClock = frame.clock;
    }
    this.cursor = (this.cursor + 1) % this.historyLength;
    this.samples = Math.min(this.samples + 1, this.historyLength);
    this.hasTelemetry = true;
    this.lastTelemetryTimestamp = now();
  }

  // Kick off the scope repaint loop once the panel is live.
  startRenderLoop() {
    if (this.frameRequest) return;
    this.frameRequest = requestAnimationFrame((timestamp) => this.renderFrame(timestamp));
  }

  // Frame limiter that keeps the scope readable without hogging the browser.
  renderFrame(timestamp) {
    this.frameRequest = requestAnimationFrame((next) => this.renderFrame(next));
    if (timestamp - this.lastRender < 1000 / TARGET_FPS) return;
    this.lastRender = timestamp;
    this.draw();
    this.updateFps(timestamp);
  }

  // Draw the background grid, EF trace, LFO traces, and peak hold marker.
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

    const efColor = 'rgba(45, 226, 230, 0.8)';
    const lfoColors = ['rgba(255, 95, 150, 0.85)', 'rgba(255, 255, 255, 0.72)'];
    this.drawLine(ctx, this.efHistory, efColor, w, h);
    this.lfoHistory.forEach((buffer, idx) => {
      const color = lfoColors[idx % lfoColors.length];
      this.drawLine(ctx, buffer, color, w, h);
    });
    this.drawTraceLabels(
      ctx,
      [
        {
          label: 'EF',
          value: this.efHistory[(this.cursor - 1 + this.historyLength) % this.historyLength],
          color: efColor
        },
        ...this.lastLfoValues.map((value, idx) => ({
          label: `LFO ${idx + 1} ${this.formatLfoConfig(this.lastLfoConfigs[idx], {
            compact: true
          })}`,
          value,
          color: lfoColors[idx % lfoColors.length]
        }))
      ],
      h
    );

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

    this.updateStatus();
    this.updateReadouts();
  }

  // Stroke one normalized history buffer across the current canvas dimensions.
  drawLine(ctx, buffer, color, width, height) {
    const sampleCount = Math.min(this.samples, this.historyLength);
    if (sampleCount < 2) return;
    const step = width / Math.max(this.historyLength, 1);
    const offsetX = Math.max(0, width - sampleCount * step);
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.beginPath();
    for (let idx = 0; idx < sampleCount; idx += 1) {
      const bufferIndex =
        (this.cursor - sampleCount + idx + this.historyLength) % this.historyLength;
      const value = clamp01(buffer[bufferIndex]);
      const x = offsetX + idx * step;
      const y = height - value * height;
      if (idx === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
    }
    ctx.stroke();
  }

  // Decide whether the peak-hold marker should be shown this frame.
  peekPeakLine(width, height) {
    if (!this.hasTelemetry || this.peakLevel <= 0) return false;
    return true;
  }

  drawTraceLabels(ctx, traces, height) {
    ctx.save();
    ctx.font = '11px ui-monospace, SFMono-Regular, Menlo, Consolas, monospace';
    ctx.textBaseline = 'middle';
    traces.forEach((trace, idx) => {
      const value = clamp01(trace.value);
      const y = Math.min(height - 12, 14 + idx * 15);
      ctx.fillStyle = trace.color;
      ctx.fillText(`${trace.label} ${value.toFixed(2)}`, 10, y);
    });
    ctx.restore();
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
