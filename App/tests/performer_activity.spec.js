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
