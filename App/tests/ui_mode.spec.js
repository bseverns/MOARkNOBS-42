import { test, expect } from '@playwright/test';

test.describe('UI mode', () => {
  test('basic mode hides advanced sections and mode persists', async ({ page }) => {
    await page.goto('/benzknobz.html');

    const basicButton = page.getByRole('button', { name: 'Basic' });
    const advancedButton = page.getByRole('button', { name: 'Advanced' });

    await expect(basicButton).toHaveAttribute('aria-pressed', 'true');
    await expect(page.locator('#filter-settings')).toBeHidden();
    await expect(page.locator('#arg-settings')).toBeHidden();
    await expect(page.locator('#scope-panel')).toBeHidden();

    await advancedButton.click();
    await expect(advancedButton).toHaveAttribute('aria-pressed', 'true');
    await expect(page.locator('#filter-settings')).toBeVisible();
    await expect(page.locator('#arg-settings')).toBeVisible();
    await expect(page.locator('#scope-panel')).toBeVisible();

    await page.reload();
    await expect(advancedButton).toHaveAttribute('aria-pressed', 'true');
    await expect(page.locator('#filter-settings')).toBeVisible();
  });

  test('basic mode still supports staged edits and apply', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    });
    await page.goto('/benzknobz.html');

    await page.getByRole('button', { name: /simulator/i }).click();
    await page.getByRole('button', { name: 'Connect' }).click();
    await expect(page.locator('.slot-editor')).toBeVisible();

    const ccInput = page.locator('.slot-editor label:has-text("CC/Note number") input').first();
    await ccInput.fill('45');
    await ccInput.dispatchEvent('change');

    await expect(page.locator('#dirty-badge')).toBeVisible();
    const apply = page.getByRole('button', { name: 'Apply' });
    await expect(apply).toBeEnabled();
    await apply.click();
    await expect(page.locator('#status-label')).toHaveText('Synced', { timeout: 5000 });
    await expect(page.locator('#dirty-badge')).toBeHidden();
  });
});
