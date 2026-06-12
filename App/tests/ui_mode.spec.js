import { test, expect } from '@playwright/test';

test.describe('UI mode', () => {
  test('basic mode hides advanced sections and mode persists', async ({ page }) => {
    await page.goto('/benzknobz.html');

    const basicButton = page.getByRole('button', { name: 'Basic' });
    const advancedButton = page.getByRole('button', { name: 'Advanced' });

    await expect(basicButton).toHaveAttribute('aria-pressed', 'true');
    await expect(page.locator('#performer-panel')).toBeVisible();
    await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Direct USB');
    await expect(page.locator('#connection-banner')).not.toContainText('Bridge');
    await expect
      .poll(async () => page.evaluate(() => window.__MN42_RUNTIME.getState().transportMode))
      .toBe('direct-webserial');
    await expect(page.locator('.runtime-lane-chip[data-runtime-lane="staged"]')).toBeVisible();
    await expect(page.locator('.runtime-lane-chip[data-runtime-lane="live"]')).toBeVisible();
    await expect(page.locator('.runtime-lane-chip[data-runtime-lane="browser"]')).toBeVisible();
    await expect(page.locator('#check-compatibility')).toBeHidden();
    await expect(page.locator('#config-mode')).toBeHidden();
    await expect(page.locator('#rollback')).toBeHidden();
    await expect(page.locator('#profile-wizard')).toBeHidden();
    await expect(page.locator('#macro-card')).toBeHidden();
    await expect(page.locator('#scene-card')).toBeHidden();
    await expect(page.locator('#stage-power-summary')).toBeHidden();
    await expect(page.locator('#stage-scene-recall')).toBeHidden();
    await expect(page.locator('#stage-panic-help')).toBeHidden();
    await expect(page.locator('#usb-midi-toggle')).toBeHidden();
    await expect(page.locator('#simulator-toggle')).toBeHidden();
    await expect(page.locator('#filter-settings')).toBeHidden();
    await expect(page.locator('#arg-settings')).toBeHidden();
    await expect(page.locator('#scope-panel')).toBeHidden();

    await advancedButton.click();
    await expect(advancedButton).toHaveAttribute('aria-pressed', 'true');
    await expect(page.locator('#performer-panel')).toBeVisible();
    await expect(page.locator('#filter-settings')).toBeVisible();
    await expect(page.locator('#arg-settings')).toBeVisible();
    await page.getByRole('button', { name: 'Scope' }).click();
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

    await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Simulator');
    await page.getByRole('button', { name: 'Connect' }).click();
    await expect(page.locator('#connection-banner')).not.toContainText('Bridge');
    await expect
      .poll(async () => page.evaluate(() => window.__MN42_RUNTIME.getState().transportMode))
      .toBe('simulator');
    await expect(page.locator('.slot-editor')).toBeVisible();
    await expect(page.locator('#export-preset')).toBeVisible();
    await expect(page.locator('#import-preset')).toBeVisible();
    await page.locator('#recovery-drawer > summary').click();
    await expect(page.locator('#profile-save')).toBeVisible();
    await expect(page.locator('#profile-load')).toBeVisible();
    await expect(page.locator('#profile-download')).toBeVisible();
    await expect(page.locator('#profile-upload')).toBeVisible();

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
