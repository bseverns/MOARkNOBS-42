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

test('removing a MIDI input binding cancels its pending field edit', async ({ page }) => {
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
  await binding.locator('summary').first().click();

  // Dispatch the edit and removal in one browser task so the 150 ms field
  // debounce cannot settle before the rendered control is destroyed.
  await binding.evaluate((detail) => {
    const destination = detail.querySelector('input[type="text"]');
    destination.value = 'arp.swing';
    destination.dispatchEvent(new Event('input', { bubbles: true }));
    detail.querySelector('.schema-array-remove').click();
  });

  await page.waitForTimeout(250);
  await expect(inputSection.locator('details')).toHaveCount(0);
  await expect
    .poll(() => page.evaluate(() => window.__MN42_RUNTIME.getState().staged.midiInputBindings))
    .toEqual([]);
});

test('rerendering an array preserves a synchronously staged select edit', async ({ page }) => {
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

  await binding.evaluate((detail) => {
    const port = detail.querySelector('select');
    port.value = 'usb';
    port.dispatchEvent(new Event('input', { bubbles: true }));
    detail.closest('[data-schema-target]').querySelector('.schema-array-add').click();
  });

  await expect
    .poll(() => page.evaluate(() => window.__MN42_RUNTIME.getState().staged.midiInputBindings))
    .toMatchObject([{ source: { port: 'usb' } }, { source: { port: 'any' } }]);
});
