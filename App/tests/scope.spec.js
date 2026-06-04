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
  const simulatorToggle = page.getByRole('button', { name: /simulator/i });
  await simulatorToggle.click();
  await page.getByRole('button', { name: 'Connect' }).click();
  await page.getByRole('button', { name: 'Scope' }).click();

  await page.waitForFunction(() => {
    const label = document.getElementById('scope-status');
    return label && /Telemetry/i.test(label.textContent ?? '');
  });
  await expect(page.locator('#scope-status')).toHaveText(/Telemetry/i, { timeout: 10000 });
  await expect(page.locator('#scope-lfo-1')).not.toHaveText('--');
  await expect(page.locator('#scope-lfo-2')).not.toHaveText('--');
  await expect(page.locator('#scope-clock')).toHaveText(/Clock (external|internal)/);

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
        <span id="scope-status"></span>
        <span id="scope-fps"></span>
        <canvas id="scope-canvas" style="width: 420px; height: 180px"></canvas>
        <span id="scope-lfo-1"></span>
        <span id="scope-lfo-2"></span>
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
        return { manifest: { lfo_count: 2 } };
      }
    };

    const panel = new ScopePanel({
      container: document.getElementById('scope-panel'),
      runtime
    });
    const pushTelemetry = listeners.get('telemetry');

    pushTelemetry({
      envelopes: [0],
      lfos: [0.75, 0.25],
      clock: { source: 'internal', running: true }
    });
    panel.draw();
    const first = {
      lfo1: document.getElementById('scope-lfo-1').textContent,
      lfo2: document.getElementById('scope-lfo-2').textContent
    };

    pushTelemetry({ envelopes: [80] });
    panel.draw();
    const afterPartial = {
      lfo1: document.getElementById('scope-lfo-1').textContent,
      lfo2: document.getElementById('scope-lfo-2').textContent
    };

    return { first, afterPartial };
  });

  expect(readouts.first).toEqual({ lfo1: '0.75', lfo2: '0.25' });
  expect(readouts.afterPartial).toEqual({ lfo1: '0.75', lfo2: '0.25' });
});
