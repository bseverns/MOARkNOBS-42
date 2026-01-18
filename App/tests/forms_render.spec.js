import { test, expect } from '@playwright/test';

test('forms render, dirty badge toggles, and apply syncs', async ({ page }) => {
  await page.addInitScript(() => {
    window.__MN42_RUNTIME_OPTIONS = {
      useSimulator: true
    };
  });

  await page.goto('/benzknobz.html');
  const simulatorToggle = page.getByRole('button', { name: /simulator/i });
  await simulatorToggle.click();
  await page.getByRole('button', { name: 'Connect' }).click();

  const freqInput = page.locator('[data-schema-target="filter"] input[type="number"]').first();
  await expect(freqInput).toHaveCount(1);
  await freqInput.fill('1234');
  await freqInput.dispatchEvent('change');

  await expect(page.locator('#dirty-badge')).toBeVisible();
  const apply = page.getByRole('button', { name: 'Apply' });
  await expect(apply).toBeEnabled();

  await apply.click();
  await expect(page.locator('#status-label')).toHaveText('Synced', { timeout: 5000 });
  await expect(page.locator('#dirty-badge')).toBeHidden();
});
