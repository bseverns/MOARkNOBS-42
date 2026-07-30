import { test, expect } from '@playwright/test';

test('slot editor preserves focused number input during live telemetry refresh', async ({
  page
}) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');
  await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Simulator');
  await page.locator('#connect').click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');
  await page.getByRole('button', { name: 'Envelope' }).click();

  const frequencyInput = page
    .locator('.slot-editor label:has-text("Tracking frequency") input')
    .first();
  await expect(frequencyInput).toBeVisible();
  await frequencyInput.fill('2345');
  await frequencyInput.focus();

  await page.waitForTimeout(700);

  await expect(frequencyInput).toHaveValue('2345');
  await expect
    .poll(() =>
      page.evaluate(() =>
        Boolean(document.activeElement?.matches('.slot-editor label input[type="number"]'))
      )
    )
    .toBe(true);
});

test('schema controls preserve focused numeric draft during config refresh', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');
  await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Simulator');
  await page.locator('#connect').click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  const freqInput = page.locator('[data-schema-target="filter"] input[type="number"]').first();
  await expect(freqInput).toBeVisible();
  await freqInput.fill('2345');
  await freqInput.focus();

  await page.evaluate(() => {
    const current = window.__MN42_RUNTIME.getState().staged;
    const next = structuredClone(current);
    next.filter = { ...(next.filter ?? {}), freq: 321 };
    window.__MN42_RUNTIME.hydrateAuthoritativeConfig(next);
  });

  await expect(freqInput).toHaveValue('2345');
  await expect
    .poll(() =>
      page.evaluate(() =>
        Boolean(
          document.activeElement?.matches('[data-schema-target="filter"] input[type="number"]')
        )
      )
    )
    .toBe(true);
});
