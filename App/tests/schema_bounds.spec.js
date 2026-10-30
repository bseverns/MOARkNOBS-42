import { test, expect } from '@playwright/test';

test('schema bounds clamp numeric inputs', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = {
      useSimulator: true
    };
  });

  await page.goto('/benzknobz.html');
  await page.locator('#cold-start-simulator').click();
  await page.locator('.workspace-tool-shell > summary').click();
  await page.getByRole('button', { name: 'EF Controls', exact: true }).click();

  const freqInput = page.locator('[data-schema-target="filter"] input[type="number"]').first();
  await expect(freqInput).toBeVisible();
  await freqInput.fill('10000');
  await freqInput.dispatchEvent('change');
  await expect(freqInput).toHaveValue('5000');
});
