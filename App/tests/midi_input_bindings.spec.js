import { test, expect } from '@playwright/test';

test('MIDI input bindings can be added, edited, and removed as staged profile config', async ({
  page
}) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/');
  await page.getByRole('button', { name: 'Connect' }).click();

  const inputSection = page.locator('[data-schema-target="midiInputBindings"]');
  await inputSection.locator('.schema-array-add').click();

  const binding = inputSection.locator('details').first();
  await expect(binding).toHaveCount(1);
  await binding.locator('summary').first().click();
  await binding.locator('input[type="number"]').nth(1).fill('74');
  await binding.locator('input[type="number"]').nth(1).dispatchEvent('change');
  await binding.locator('input[type="text"]').fill('arp.swing');
  await binding.locator('input[type="text"]').dispatchEvent('change');

  await expect(page.locator('#dirty-badge')).toBeVisible();
  await expect
    .poll(() =>
      page.evaluate(() => window.__MN42_RUNTIME.getState().staged.midiInputBindings?.[0])
    )
    .toMatchObject({
      source: { port: 'any', type: 'cc7', channel: 1, number: 74 },
      destination: 'arp.swing',
      mode: 'absolute',
      outputRange: [0, 127],
      pickup: 'soft'
    });

  await inputSection.getByRole('button', { name: 'Remove MIDI Input Bindings 1' }).click();
  await expect(inputSection.locator('details')).toHaveCount(0);
});
