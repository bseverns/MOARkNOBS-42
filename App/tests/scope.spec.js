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
  await expect(page.locator('#scope-ef-legend [data-state="active"]')).toHaveCount(3);
  await expect(page.locator('#scope-panel [data-scope-role="view-state"]')).toHaveText(
    /VIEW: ACTIVE · 3\/6 EFs · 2 LFOs always visible/
  );
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
