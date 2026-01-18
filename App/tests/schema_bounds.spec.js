import { test, expect } from '@playwright/test';

test('schema bounds clamp numeric inputs', async ({ page }) => {
  await page.addInitScript(() => {
    window.__MN42_RUNTIME_OPTIONS = {
      useSimulator: true
    };
  });

  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: /simulator/i }).click();
  await page.getByRole('button', { name: 'Connect' }).click();

  const freqInput = page.locator('[data-schema-target="filter"] input[type="number"]').first();
  await freqInput.fill('10000');
  await freqInput.dispatchEvent('change');
  await expect(freqInput).toHaveValue('5000');
});
