import { test, expect } from '@playwright/test';

test('scope panel streams telemetry and emits snapshots', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = {
      useSimulator: true
    };
    window.__mn42ScopeBlob = null;
    const originalCreate = URL.createObjectURL.bind(URL);
    URL.createObjectURL = (blob) => {
      window.__mn42ScopeBlob = blob;
      return originalCreate(blob);
    };
  });

  await page.goto('/benzknobz.html');
  const simulatorToggle = page.locator('#simulator-toggle');
  await simulatorToggle.click();
  await page.getByRole('button', { name: 'Connect' }).click();
  await page.getByRole('tab', { name: 'Scope' }).click();

  await page.waitForFunction(() => {
    const label = document.getElementById('scope-status');
    return label && /Telemetry/i.test(label.textContent ?? '');
  });
  await expect(page.locator('#scope-status')).toHaveText(/Telemetry/i, { timeout: 10000 });
  await expect(page.locator('#scope-lfo-1')).not.toHaveText('--');
  await expect(page.locator('#scope-lfo-2')).not.toHaveText('--');
  await expect(page.locator('#scope-clock')).toHaveText(/Clock (external|internal)/);
  await expect(page.locator('#scope-ef-legend [data-ef-index]')).toHaveCount(6);
  await expect
    .poll(() => page.locator('#scope-ef-legend [data-state="active"]').count())
    .toBeGreaterThan(0);
  await expect(page.locator('#scope-panel [data-scope-role="view-state"]')).toHaveText(
    /VIEW: ACTIVE · \d+\/6 EFs(?: \([^)]*\))? · 2 LFOs always visible/
  );
  await expect(page.locator('#scope-panel [data-scope-window="5"]')).toHaveAttribute(
    'aria-pressed',
    'true'
  );
  await page.locator('#scope-panel [data-scope-window="2"]').click();
  await expect(page.locator('#scope-panel [data-scope-window="2"]')).toHaveAttribute(
    'aria-pressed',
    'true'
  );
  await expect(page.locator('#scope-canvas')).toHaveAttribute('aria-label', /over 2 seconds/);
  await expect(page.locator('#scope-panel [data-scope-view="active"]')).toHaveAttribute(
    'aria-pressed',
    'true'
  );

  await page.locator('#scope-panel [data-scope-view="all"]').click();
  await expect(page.locator('#scope-panel [data-scope-view="all"]')).toHaveAttribute(
    'aria-pressed',
    'true'
  );
  await page.locator('#scope-ef-legend [data-ef-index="1"]').click();
  await expect(page.locator('#scope-ef-legend [data-ef-index="1"]')).toHaveAttribute(
    'aria-pressed',
    'true'
  );
  await expect(page.locator('#scope-panel [data-scope-role="view-state"]')).toHaveText(
    /VIEW: ALL · FOCUS: EF 2 · \d+/
  );
  await expect(page.locator('#scope-panel [data-scope-view="all"]')).toHaveAttribute(
    'aria-pressed',
    'false'
  );
  await page.locator('#scope-panel [data-scope-role="leave-solo"]').click();
  await expect(page.locator('#scope-panel [data-scope-view="all"]')).toHaveAttribute(
    'aria-pressed',
    'true'
  );

  await page.getByRole('button', { name: 'Refresh scope' }).click();
  await expect(page.locator('#scope-status')).toHaveText(/Waiting for telemetry|Telemetry/i);

  await page.getByRole('button', { name: 'PNG snapshot' }).click();
  await page.waitForFunction(() => window.__mn42ScopeBlob instanceof Blob, { timeout: 5000 });
  const snapshot = await page.evaluate(() => ({
    size: window.__mn42ScopeBlob?.size ?? 0,
    type: window.__mn42ScopeBlob?.type ?? ''
  }));
  expect(snapshot.size).toBeGreaterThan(0);
  expect(snapshot.type).toBe('image/png');
});

test('scope panel keeps LFO readouts across partial telemetry frames', async ({ page }) => {
  await page.goto('/views/scope_panel.js');

  const readouts = await page.evaluate(async () => {
    const { ScopePanel } = await import('/views/scope_panel.js');
    document.body.innerHTML = `
      <section id="scope-panel" style="width: 420px">
        <button id="scope-snapshot">PNG snapshot</button>
        <button id="scope-refresh">Refresh scope</button>
        <span id="scope-status"></span>
        <span id="scope-fps"></span>
        <strong data-scope-role="view-state"></strong>
        <button data-scope-role="leave-solo" hidden>Leave solo</button>
        <button data-scope-view="active" aria-pressed="true">Active EFs</button>
        <button data-scope-view="all" aria-pressed="false">All EFs</button>
        <canvas id="scope-canvas" style="width: 420px; height: 180px"></canvas>
        <div id="scope-ef-legend"></div>
        <span class="scope-legend-item lfo-one">LFO 1 <b id="scope-lfo-1"></b></span>
        <span class="scope-legend-item lfo-two">LFO 2 <b id="scope-lfo-2"></b></span>
        <span id="scope-clock"></span>
      </section>
    `;

    const listeners = new Map();
    const runtime = {
      on(event, callback) {
        listeners.set(event, callback);
        return () => listeners.delete(event);
      },
      getState() {
        return { manifest: { envelope_count: 3, lfo_count: 2 } };
      }
    };

    const panel = new ScopePanel({
      container: document.getElementById('scope-panel'),
      runtime
    });
    const pushTelemetry = listeners.get('telemetry');

    pushTelemetry({
      envelopes: [10, 80, 30],
      efStatus: [1, 0, 1],
      lfos: [0.75, 0.25],
      lfo_config: [
        {
          index: 0,
          shape: 2,
          shape_name: 'Saw',
          frequency_hz: 1,
          depth: 0.75,
          bipolar: false,
          sync: true,
          sync_ratio: 3,
          sync_ratio_name: '1/8'
        }
      ],
      clock: { source: 'internal', running: true }
    });
    panel.draw();
    const first = {
      lfo1: document.getElementById('scope-lfo-1').textContent,
      lfo2: document.getElementById('scope-lfo-2').textContent,
      lfo1Legend: document.querySelector('.scope-legend-item.lfo-one').textContent
    };

    const activeEfIndices = panel.visibleEfIndices();
    pushTelemetry({ envelopes: [20] });
    panel.draw();
    const afterPartial = {
      lfo1: document.getElementById('scope-lfo-1').textContent,
      lfo2: document.getElementById('scope-lfo-2').textContent
    };

    const lastHistoryIndex = (panel.cursor - 1 + panel.historyLength) % panel.historyLength;
    const inactiveHistoryWasPreserved = Math.round(panel.efHistory[1][lastHistoryIndex] * 127);
    document.querySelector('[data-ef-index="1"]').click();
    const soloEfIndices = panel.visibleEfIndices();
    const soloState = document.querySelector('[data-scope-role="view-state"]').textContent;
    const soloFocus = document.querySelector('[data-ef-index="1"]').dataset.focus;
    document.querySelector('[data-scope-role="leave-solo"]').click();
    document.querySelector('[data-scope-view="all"]').click();
    const allEfIndices = panel.visibleEfIndices();

    return {
      first,
      afterPartial,
      activeEfIndices,
      soloEfIndices,
      soloState,
      soloFocus,
      allEfIndices,
      inactiveHistoryWasPreserved,
      efLegendStates: Array.from(document.querySelectorAll('[data-ef-index]')).map(
        (element) => element.dataset.state
      )
    };
  });

  expect(readouts.first.lfo1).toBe('0.75');
  expect(readouts.first.lfo2).toBe('0.25');
  expect(readouts.first.lfo1Legend).toContain('Saw');
  expect(readouts.first.lfo1Legend).toContain('1/8');
  expect(readouts.afterPartial).toEqual({ lfo1: '0.75', lfo2: '0.25' });
  expect(readouts.activeEfIndices).toEqual([0, 2]);
  expect(readouts.soloEfIndices).toEqual([1]);
  expect(readouts.soloState).toContain('VIEW: ACTIVE · FOCUS: EF 2 · 80');
  expect(readouts.soloFocus).toBe('soloed');
  expect(readouts.allEfIndices).toEqual([0, 1, 2]);
  expect(readouts.inactiveHistoryWasPreserved).toBe(80);
  expect(readouts.efLegendStates).toEqual(['active', 'inactive', 'active']);
});

test('active EF view holds recent activity before showing its intentional empty state', async ({
  page
}) => {
  await page.goto('/views/scope_panel.js');

  const states = await page.evaluate(async () => {
    const { ScopePanel } = await import('/views/scope_panel.js');
    document.body.innerHTML = `
      <details id="scope-drawer">
        <summary>Motion · <span data-scope-summary></span></summary>
        <section id="scope-panel" style="width: 420px">
          <strong data-scope-role="view-state"></strong>
          <button data-scope-role="leave-solo" hidden>Leave solo</button>
          <button data-scope-view="active" aria-pressed="true">Active</button>
          <button data-scope-view="all" aria-pressed="false">All</button>
          <canvas data-scope-role="canvas" width="420" height="180"
            style="width: 420px; height: 180px"></canvas>
          <div data-scope-role="ef-legend"></div>
        </section>
      </details>
    `;
    let clock = 1000;
    const listeners = new Map();
    const runtime = {
      on(event, callback) {
        listeners.set(event, callback);
        return () => listeners.delete(event);
      },
      getState() {
        return { manifest: { envelope_count: 2, lfo_count: 2 } };
      }
    };
    const container = document.getElementById('scope-panel');
    const panel = new ScopePanel({
      container,
      runtime,
      renderToggle: document.getElementById('scope-drawer'),
      activityHoldMs: 1500,
      nowFn: () => clock
    });
    const pushTelemetry = listeners.get('telemetry');
    pushTelemetry({ envelopes: [90, 10], efStatus: [1, 0], lfos: [0.2, 0.8] });
    clock = 1100;
    pushTelemetry({ envelopes: [70, 10], efStatus: [0, 0], lfos: [0.3, 0.7] });
    const recent = {
      visible: panel.visibleEfIndices(),
      opacity: panel.efTraceOpacity(0),
      legendState: container.querySelector('[data-ef-index="0"]').dataset.state,
      summary: container.querySelector('[data-scope-role="view-state"]').textContent,
      stageSummary: document.querySelector('[data-scope-summary]').textContent
    };
    clock = 2601;
    panel.draw();
    const expired = {
      visible: panel.visibleEfIndices(),
      opacity: panel.efTraceOpacity(0),
      legendState: container.querySelector('[data-ef-index="0"]').dataset.state,
      summary: container.querySelector('[data-scope-role="view-state"]').textContent,
      stageSummary: document.querySelector('[data-scope-summary]').textContent,
      empty: container.querySelector('canvas').dataset.efEmpty,
      ariaLabel: container.querySelector('canvas').getAttribute('aria-label')
    };
    panel.destroy();
    return { recent, expired };
  });

  expect(states.recent.visible).toEqual([0]);
  expect(states.recent.opacity).toBeGreaterThan(states.expired.opacity);
  expect(states.recent.legendState).toBe('recent');
  expect(states.recent.summary).toContain('1/2 EFs (0 active, 1 recent)');
  expect(states.recent.stageSummary).toBe('1 EF recently active · 2 LFOs');
  expect(states.expired.visible).toEqual([]);
  expect(states.expired.legendState).toBe('inactive');
  expect(states.expired.summary).toContain('No active EFs · 2 LFOs still running');
  expect(states.expired.stageSummary).toBe('No active EFs · 2 LFOs still running');
  expect(states.expired.empty).toBe('true');
  expect(states.expired.ariaLabel).toContain('histories are still recording');
});

test('scope X zoom uses elapsed time rather than a fixed sample fraction', async ({ page }) => {
  await page.goto('/views/scope_panel.js');

  const windows = await page.evaluate(async () => {
    const { ScopePanel } = await import('/views/scope_panel.js');
    document.body.innerHTML = `
      <section id="scope-panel" style="width: 420px">
        <button data-scope-window="2" aria-pressed="false">2s</button>
        <button data-scope-window="5" aria-pressed="true">5s</button>
        <button data-scope-window="10" aria-pressed="false">10s</button>
        <canvas data-scope-role="canvas" width="420" height="180"
          style="width: 420px; height: 180px"></canvas>
      </section>
    `;
    let clock = 1000;
    const listeners = new Map();
    const runtime = {
      on(event, callback) {
        listeners.set(event, callback);
        return () => listeners.delete(event);
      },
      getState() {
        return { manifest: { envelope_count: 1, lfo_count: 2 } };
      }
    };
    const container = document.getElementById('scope-panel');
    const panel = new ScopePanel({ container, runtime, nowFn: () => clock });
    const pushTelemetry = listeners.get('telemetry');
    pushTelemetry({ envelopes: [10], lfos: [0.1, 0.9] });
    clock = 3000;
    pushTelemetry({ envelopes: [40], lfos: [0.4, 0.6] });
    clock = 5500;
    pushTelemetry({ envelopes: [90], lfos: [0.9, 0.1] });

    const timestampsForWindow = (seconds) => {
      panel.setTimeWindow(seconds);
      return panel.visibleSampleIndices().map((idx) => panel.timestampHistory[idx]);
    };
    const result = {
      twoSeconds: timestampsForWindow(2),
      fiveSeconds: timestampsForWindow(5),
      tenSeconds: timestampsForWindow(10),
      selected: Array.from(container.querySelectorAll('[data-scope-window]')).map((button) =>
        button.getAttribute('aria-pressed')
      ),
      ariaLabel: container.querySelector('canvas').getAttribute('aria-label')
    };
    panel.destroy();
    return result;
  });

  expect(windows.twoSeconds).toEqual([5500]);
  expect(windows.fiveSeconds).toEqual([1000, 3000, 5500]);
  expect(windows.tenSeconds).toEqual([1000, 3000, 5500]);
  expect(windows.selected).toEqual(['false', 'false', 'true']);
  expect(windows.ariaLabel).toContain('over 10 seconds');
});

test('simulator EF rehearsal signals move smoothly and derive musical activity state', async ({
  page
}) => {
  await page.goto('/runtime/simulator_transport.js');

  const samples = await page.evaluate(async () => {
    const { createSimulator } = await import('/runtime/simulator_transport.js');
    const simulator = createSimulator({
      createManifest: () => ({ slot_count: 42, pot_count: 42, envelope_count: 6 }),
      argMethodNames: ['PLUS'],
      efFilterNames: ['Linear'],
      cloneValue: structuredClone,
      setNested: () => {},
      telemetryFrameMs: 0
    });
    await simulator.open();
    const frames = [];
    for (let idx = 0; idx < 48; idx += 1) {
      const frame = JSON.parse(await simulator.nextLine());
      frames.push({ envelopes: frame.envelopes, efStatus: frame.efStatus });
    }
    await simulator.close();
    return frames;
  });

  const values = samples.map((frame) => frame.envelopes[0]);
  const deltas = values.slice(1).map((value, idx) => Math.abs(value - values[idx]));
  expect(Math.max(...deltas)).toBeLessThan(24);
  expect(Math.max(...deltas)).toBeGreaterThan(0);
  expect(samples.every((frame) => frame.envelopes[1] <= 13 && frame.efStatus[1] === 0)).toBe(true);
  expect(samples.some((frame) => frame.efStatus[2] === 1)).toBe(true);
  expect(samples.some((frame) => frame.efStatus[2] === 0)).toBe(true);
  expect(
    samples.every((frame) => frame.efStatus[2] === (frame.envelopes[2] >= 42 ? 1 : 0))
  ).toBe(true);
});

test('timestamp history preserves chronological order across ring wrap and resize', async ({
  page
}) => {
  await page.goto('/views/scope_panel.js');

  const history = await page.evaluate(async () => {
    const { ScopePanel } = await import('/views/scope_panel.js');
    document.body.innerHTML = `
      <section id="scope-panel" style="width: 64px">
        <canvas data-scope-role="canvas" width="64" height="100"
          style="width: 64px; height: 100px"></canvas>
      </section>
    `;
    let clock = 1000;
    const listeners = new Map();
    const runtime = {
      on(event, callback) {
        listeners.set(event, callback);
        return () => listeners.delete(event);
      },
      getState() {
        return { manifest: { envelope_count: 1, lfo_count: 2 } };
      }
    };
    const panel = new ScopePanel({
      container: document.getElementById('scope-panel'),
      runtime,
      nowFn: () => clock,
      timeWindowSeconds: 10
    });
    const pushTelemetry = listeners.get('telemetry');
    const orderedTimestamps = () => {
      const count = Math.min(panel.samples, panel.historyLength);
      return Array.from({ length: count }, (_, idx) => {
        const bufferIndex =
          (panel.cursor - count + idx + panel.historyLength) % panel.historyLength;
        return panel.timestampHistory[bufferIndex];
      });
    };

    for (let idx = 0; idx < 80; idx += 1) {
      clock = 1000 + idx * 100;
      pushTelemetry({ envelopes: [idx], lfos: [0.2, 0.8] });
    }
    const wrapped = {
      length: panel.historyLength,
      samples: panel.samples,
      timestamps: orderedTimestamps()
    };

    panel.resizeHistoryBuffers(96);
    const grown = {
      length: panel.historyLength,
      samples: panel.samples,
      timestamps: orderedTimestamps()
    };
    for (let idx = 80; idx < 120; idx += 1) {
      clock = 1000 + idx * 100;
      pushTelemetry({ envelopes: [idx], lfos: [0.2, 0.8] });
    }
    const grownAndWrapped = {
      samples: panel.samples,
      timestamps: orderedTimestamps()
    };
    panel.resizeHistoryBuffers(64);
    const shrunk = {
      length: panel.historyLength,
      samples: panel.samples,
      timestamps: orderedTimestamps()
    };
    panel.destroy();
    return { wrapped, grown, grownAndWrapped, shrunk };
  });

  expect(history.wrapped.length).toBe(64);
  expect(history.wrapped.samples).toBe(64);
  expect(history.wrapped.timestamps).toEqual(
    Array.from({ length: 64 }, (_, idx) => 2600 + idx * 100)
  );
  expect(history.grown.length).toBe(96);
  expect(history.grown.samples).toBe(64);
  expect(history.grown.timestamps).toEqual(history.wrapped.timestamps);
  expect(history.grownAndWrapped.samples).toBe(96);
  expect(history.grownAndWrapped.timestamps).toEqual(
    Array.from({ length: 96 }, (_, idx) => 3400 + idx * 100)
  );
  expect(history.shrunk.length).toBe(64);
  expect(history.shrunk.samples).toBe(64);
  expect(history.shrunk.timestamps).toEqual(
    Array.from({ length: 64 }, (_, idx) => 6600 + idx * 100)
  );
});

test('role-based scope records while closed and only renders when its drawer is open', async ({
  page
}) => {
  await page.goto('/views/scope_panel.js');

  const state = await page.evaluate(async () => {
    const { ScopePanel } = await import('/views/scope_panel.js');
    document.body.innerHTML = `
      <details id="drawer">
        <summary>Motion</summary>
        <section id="motion-panel">
          <button data-scope-view="active" aria-pressed="true">Active EFs</button>
          <button data-scope-view="all" aria-pressed="false">All EFs</button>
          <canvas data-scope-role="canvas" width="420" height="180"
            style="width: 420px; height: 180px"></canvas>
          <span data-scope-role="status"></span>
          <div data-scope-role="ef-legend"></div>
          <span class="scope-legend-item">LFO 1 <b data-scope-lfo-index="0"></b></span>
          <span class="scope-legend-item">LFO 2 <b data-scope-lfo-index="1"></b></span>
          <span data-scope-role="clock"></span>
        </section>
      </details>
    `;
    const listeners = new Map();
    const runtime = {
      on(event, callback) {
        listeners.set(event, callback);
        return () => listeners.delete(event);
      },
      getState() {
        return { manifest: { envelope_count: 2, lfo_count: 2 } };
      }
    };
    const drawer = document.getElementById('drawer');
    const container = document.getElementById('motion-panel');
    const panel = new ScopePanel({ container, runtime, renderToggle: drawer });
    const pushTelemetry = listeners.get('telemetry');
    pushTelemetry({ envelopes: [10, 80], efStatus: [1, 0], lfos: [0.2, 0.8] });
    pushTelemetry({ envelopes: [20, 70], efStatus: [1, 0], lfos: [0.3, 0.7] });
    pushTelemetry({ envelopes: [30, 60], efStatus: [1, 0], lfos: [0.4, 0.6] });
    const closed = {
      rendering: container.dataset.scopeRendering,
      samples: panel.samples,
      frameRequest: panel.frameRequest
    };

    drawer.open = true;
    drawer.dispatchEvent(new Event('toggle'));
    const opened = {
      rendering: container.dataset.scopeRendering,
      samples: panel.samples,
      lfo1: container.querySelector('[data-scope-lfo-index="0"]').textContent,
      efCount: container.querySelectorAll('[data-ef-index]').length
    };

    drawer.open = false;
    drawer.dispatchEvent(new Event('toggle'));
    const closedAgain = {
      rendering: container.dataset.scopeRendering,
      samples: panel.samples,
      frameRequest: panel.frameRequest
    };
    panel.destroy();
    return { closed, opened, closedAgain };
  });

  expect(state.closed).toEqual({ rendering: 'false', samples: 3, frameRequest: null });
  expect(state.opened.rendering).toBe('true');
  expect(state.opened.samples).toBe(3);
  expect(state.opened.lfo1).toBe('0.40');
  expect(state.opened.efCount).toBe(2);
  expect(state.closedAgain).toEqual({ rendering: 'false', samples: 3, frameRequest: null });
});
