const DEFAULT_LFO_COUNT = 2;
const MIN_HISTORY = 64;
const TARGET_FPS = 30;
const PEAK_DECAY = 0.003;

const clamp01 = (value) => {
  const num = Number(value);
  if (!Number.isFinite(num)) return 0;
  return Math.min(1, Math.max(0, num));
};

const now = () => (typeof performance === 'object' && typeof performance.now === 'function' ? performance.now() : Date.now());

export class ScopePanel {
  constructor({ container, runtime, manifest } = {}) {
    this.container = container;
    if (!container || typeof window === 'undefined') return;

    this.canvas = container.querySelector('#scope-canvas');
    this.statusLabel = container.querySelector('#scope-status');
    this.fpsLabel = container.querySelector('#scope-fps');
    this.snapshotButton = container.querySelector('#scope-snapshot');
    if (!this.canvas || !this.canvas.getContext) return;

    this.ctx = this.canvas.getContext('2d');
    this.runtime = runtime;
    this.historyLength = Math.max(MIN_HISTORY, Math.round(this.canvas.width || MIN_HISTORY));
    this.efHistory = new Float32Array(this.historyLength);
    this.lfoCount = DEFAULT_LFO_COUNT;
    this.lfoHistory = Array.from({ length: this.lfoCount }, () => new Float32Array(this.historyLength));
    this.cursor = 0;
    this.samples = 0;
    this.peakLevel = 0;
    this.hasTelemetry = false;
    this.lastTelemetryTimestamp = null;
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

  applyManifest(manifest = {}) {
    const lfoCountCandidate = Number(manifest?.lfo_count);
    const lfoCount = Number.isFinite(lfoCountCandidate) ? Math.max(0, Math.floor(lfoCountCandidate)) : DEFAULT_LFO_COUNT;
    if (lfoCount === this.lfoCount) return;
    this.lfoCount = Math.max(DEFAULT_LFO_COUNT, lfoCount);
    this.initializeBuffers();
  }

  initializeBuffers() {
    this.efHistory = new Float32Array(this.historyLength);
    this.lfoHistory = Array.from({ length: this.lfoCount }, () => new Float32Array(this.historyLength));
    this.cursor = 0;
    this.samples = 0;
    this.peakLevel = 0;
  }

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

  handleTelemetry(frame = {}) {
    if (!this.ctx) return;
    const envelopes = Array.isArray(frame.envelopes) ? frame.envelopes : [];
    const lfos = Array.isArray(frame.lfos) ? frame.lfos : [];
    const aggregated = envelopes.length
      ? Math.max(...envelopes.map((value) => clamp01((Number(value) || 0) / 127)))
      : 0;
    this.efHistory[this.cursor] = aggregated;
    this.peakLevel = Math.max(this.peakLevel, aggregated);
    this.lfoHistory.forEach((buffer, idx) => {
      const value = clamp01(Number(lfos[idx]) || 0);
      buffer[this.cursor] = value;
    });
    this.cursor = (this.cursor + 1) % this.historyLength;
    this.samples = Math.min(this.samples + 1, this.historyLength);
    this.hasTelemetry = true;
    this.lastTelemetryTimestamp = now();
  }

  startRenderLoop() {
    if (this.frameRequest) return;
    this.frameRequest = requestAnimationFrame((timestamp) => this.renderFrame(timestamp));
  }

  renderFrame(timestamp) {
    this.frameRequest = requestAnimationFrame((next) => this.renderFrame(next));
    if (timestamp - this.lastRender < 1000 / TARGET_FPS) return;
    this.lastRender = timestamp;
    this.draw();
    this.updateFps(timestamp);
  }

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

    this.drawLine(ctx, this.efHistory, 'rgba(45, 226, 230, 0.8)', w, h);
    const lfoColors = ['rgba(255, 95, 150, 0.8)', 'rgba(255, 255, 255, 0.65)'];
    this.lfoHistory.forEach((buffer, idx) => {
      const color = lfoColors[idx % lfoColors.length];
      this.drawLine(ctx, buffer, color, w, h);
    });

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
  }

  drawLine(ctx, buffer, color, width, height) {
    const sampleCount = Math.min(this.samples, this.historyLength);
    if (sampleCount < 2) return;
    const step = width / Math.max(this.historyLength, 1);
    const offsetX = Math.max(0, width - sampleCount * step);
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.beginPath();
    for (let idx = 0; idx < sampleCount; idx += 1) {
      const bufferIndex = (this.cursor - sampleCount + idx + this.historyLength) % this.historyLength;
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

  peekPeakLine(width, height) {
    if (!this.hasTelemetry || this.peakLevel <= 0) return false;
    return true;
  }

  updateStatus() {
    if (!this.statusLabel) return;
    if (!this.hasTelemetry) {
      this.statusLabel.textContent = 'Waiting for telemetry…';
      if (this.snapshotButton) this.snapshotButton.disabled = true;
      return;
    }
    if (this.snapshotButton) this.snapshotButton.disabled = false;
    const age = this.lastTelemetryTimestamp ? Math.max(0, Math.round(now() - this.lastTelemetryTimestamp)) : 0;
    this.statusLabel.textContent = age ? `Telemetry ${age} ms ago` : 'Telemetry streaming…';
  }

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

  downloadDataUrl(dataUrl) {
    const name = `mn42-scope-${new Date().toISOString().replace(/[:.]/g, '-')}.png`;
    const anchor = document.createElement('a');
    anchor.href = dataUrl;
    anchor.download = name;
    anchor.click();
  }
}
