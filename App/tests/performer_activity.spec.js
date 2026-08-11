import { test, expect } from '@playwright/test';

test('Stage slot activity pulses on change and decays while a value remains steady', async ({
  page
}) => {
  await page.goto('/views/controllers/performer_panel_controller.js');

  const states = await page.evaluate(async () => {
    const { createPerformerPanelController } = await import(
      '/views/controllers/performer_panel_controller.js'
    );
    document.body.innerHTML = '<div id="slots"></div>';
    const controller = createPerformerPanelController({
      runtime: { getState: () => ({}) },
      elements: { slotGrid: document.getElementById('slots') },
      slotTypeAbbreviations: { CC: 'CC' }
    });
    controller.renderSlots([{ type: 'CC' }]);
    const cell = document.querySelector('.stage-slot-cell');

    controller.paintTelemetry({ slots: [64] });
    const baseline = cell.classList.contains('active');
    controller.paintTelemetry({ slots: [64] });
    const steadyBaseline = cell.classList.contains('active');
    controller.paintTelemetry({ slots: [65] });
    const changed = cell.classList.contains('active');
    controller.paintTelemetry({ slots: [65] });
    const steadyDuringPulse = cell.classList.contains('active');
    await new Promise((resolve) => setTimeout(resolve, 350));
    const decayed = cell.classList.contains('active');

    return { baseline, steadyBaseline, changed, steadyDuringPulse, decayed };
  });

  expect(states).toEqual({
    baseline: false,
    steadyBaseline: false,
    changed: true,
    steadyDuringPulse: true,
    decayed: false
  });
});

test('Stage preserves slot output while envelope-only frames update source context', async ({
  page
}) => {
  await page.goto('/views/controllers/performer_panel_controller.js');

  const state = await page.evaluate(async () => {
    const { createPerformerPanelController } = await import(
      '/views/controllers/performer_panel_controller.js'
    );
    document.body.innerHTML = `
      <div id="slots"></div>
      <div id="envelopes"></div>
      <div id="focus"></div>
      <div id="clock"></div>
    `;
    const runtimeState = {
      staged: {
        slots: [{ label: 'Filter cutoff' }],
        efSlots: [
          { index: 0, slots: [0] },
          { index: 1, slots: [] }
        ]
      }
    };
    const controller = createPerformerPanelController({
      runtime: { getState: () => runtimeState },
      elements: {
        slotGrid: document.getElementById('slots'),
        envelopeContainer: document.getElementById('envelopes'),
        slotFocus: document.getElementById('focus'),
        clockState: document.getElementById('clock')
      },
      slotTypeAbbreviations: { CC: 'CC' }
    });
    controller.rebuildMeters(2);
    controller.renderSlots([
      {
        type: 'CC',
        label: 'Filter cutoff',
        channel: 3,
        data1: 74,
        ef_index: 0,
        lfo: [{ enabled: true }, { enabled: false }]
      }
    ]);
    controller.paintTelemetry({
      slots: [91],
      envelopes: [68, 4],
      efStatus: [1, 0],
      lfos: [0.73, 0.2],
      clock: {
        source: 'external',
        external_bpm: 123.8,
        running: true
      }
    });
    controller.paintTelemetry({ envelopes: [72, 5], efStatus: [1, 0] });
    const routeButton = document.querySelector('#envelopes .meter-routes');
    routeButton.click();

    return {
      focus: document.getElementById('focus').textContent,
      clock: document.getElementById('clock').textContent,
      meterStates: Array.from(document.querySelectorAll('#envelopes .meter')).map(
        (meter) => meter.dataset.state
      ),
      meterText: document.querySelector('#envelopes .meter')?.textContent,
      routeExpanded: routeButton.getAttribute('aria-expanded'),
      routeDestinations: document.querySelector('#envelopes .meter-destinations')?.textContent,
      slotName: document.querySelector('.stage-slot-name')?.textContent,
      slotAriaLabel: document.querySelector('.stage-slot-cell')?.getAttribute('aria-label'),
      modulationColors: Array.from(document.querySelectorAll('.modulation-badge')).map((badge) =>
        badge.style.getPropertyValue('--modulation-color')
      )
    };
  });

  expect(state.focus).toBe('Filter cutoff · S1 · CC74 · Ch 3 · OUT 91 · E1 72 · L1 0.73');
  expect(state.clock).toBe('EXT · 123.8 BPM · Running');
  expect(state.meterStates).toEqual(['active', 'inactive']);
  expect(state.meterText).toContain('ACTIVE');
  expect(state.meterText).toContain('→ 1 slot');
  expect(state.routeExpanded).toBe('true');
  expect(state.routeDestinations).toBe('Filter cutoff (S1)');
  expect(state.slotName).toBe('Filter cutoff');
  expect(state.slotAriaLabel).toBe('Filter cutoff, slot 1');
  expect(state.modulationColors).toHaveLength(2);
  expect(state.modulationColors[0]).not.toBe(state.modulationColors[1]);
});

test('Stage clock distinguishes external lock from explicit internal fallback', async ({ page }) => {
  await page.goto('/views/controllers/performer_panel_controller.js');

  const states = await page.evaluate(async () => {
    const { createPerformerPanelController } = await import(
      '/views/controllers/performer_panel_controller.js'
    );
    document.body.innerHTML = '<strong id="clock"></strong>';
    const clock = document.getElementById('clock');
    const controller = createPerformerPanelController({
      runtime: { getState: () => ({}) },
      elements: { clockState: clock }
    });

    controller.paintTelemetry({
      clock: {
        follow_external: true,
        source: 'internal',
        tapped_bpm: 120,
        external_bpm: 0,
        external_signal: false,
        running: true
      }
    });
    const waiting = { text: clock.textContent, health: clock.dataset.clockHealth };

    controller.paintTelemetry({ clock: { running: false } });
    const partial = { text: clock.textContent, health: clock.dataset.clockHealth };

    controller.paintTelemetry({
      clock: {
        source: 'external',
        external_bpm: 128.25,
        external_signal: true,
        running: true
      }
    });
    const locked = { text: clock.textContent, health: clock.dataset.clockHealth };

    return { waiting, partial, locked };
  });

  expect(states).toEqual({
    waiting: {
      text: 'INT fallback · 120.0 BPM · Waiting for EXT',
      health: 'waiting-external'
    },
    partial: {
      text: 'INT fallback · 120.0 BPM · Waiting for EXT',
      health: 'waiting-external'
    },
    locked: { text: 'EXT · 128.3 BPM · Running', health: 'external' }
  });
});

test('Stage telemetry health distinguishes waiting and last-known readings', async ({ page }) => {
  await page.goto('/views/controllers/performer_panel_controller.js');

  const states = await page.evaluate(async () => {
    const { formatStageTelemetryHealth } = await import(
      '/views/controllers/performer_panel_controller.js'
    );
    return {
      offline: formatStageTelemetryHealth({ freshness: 'live', ageMs: 10 }, { connected: false }),
      waiting: formatStageTelemetryHealth(
        { freshness: 'stale', receivedAt: null, ageMs: null },
        { connected: true }
      ),
      live: formatStageTelemetryHealth(
        { freshness: 'live', receivedAt: 1000, ageMs: 20 },
        { connected: true }
      ),
      delayed: formatStageTelemetryHealth(
        { freshness: 'delayed', receivedAt: 1000, ageMs: 1520 },
        { connected: true }
      ),
      stale: formatStageTelemetryHealth(
        { freshness: 'stale', receivedAt: 1000, ageMs: 3260 },
        { connected: true }
      )
    };
  });

  expect(states).toEqual({
    offline: { text: 'Telemetry offline', freshness: 'offline' },
    waiting: { text: 'Telemetry waiting', freshness: 'waiting' },
    live: { text: 'Telemetry live', freshness: 'live' },
    delayed: { text: 'Telemetry delayed · 1.5s', freshness: 'delayed' },
    stale: { text: 'Telemetry stale · 3.3s', freshness: 'stale' }
  });
});

test('slot workspace forwards partial telemetry frames and preserves its merged snapshot', async ({
  page
}) => {
  await page.goto('/views/controllers/slot_workspace_controller.js');

  const state = await page.evaluate(async () => {
    const { createSlotWorkspaceController } = await import(
      '/views/controllers/slot_workspace_controller.js'
    );
    const slotState = { selected: 0, slots: [], efSlots: [], telemetry: null };
    const forwarded = [];
    const controller = createSlotWorkspaceController({
      runtime: {},
      slotState,
      performerPanel: {
        paintTelemetry(frame) {
          forwarded.push(frame);
        }
      }
    });

    controller.paintTelemetry({ slots: [91], envelopes: [68, 4], lfos: [0.73, 0.2] });
    controller.paintTelemetry({ envelopes: [72, 5], efStatus: [1, 0] });

    return {
      forwarded,
      telemetry: slotState.telemetry
    };
  });

  expect(state.forwarded).toHaveLength(2);
  expect(state.forwarded[1]).toEqual({ envelopes: [72, 5], efStatus: [1, 0] });
  expect(state.telemetry).toMatchObject({
    slots: [91],
    envelopes: [72, 5],
    efStatus: [1, 0],
    lfos: [0.73, 0.2]
  });
});
