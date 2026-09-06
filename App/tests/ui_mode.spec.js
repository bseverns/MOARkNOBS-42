import { test, expect } from '@playwright/test';

test.describe('UI mode', () => {
  test('basic mode hides advanced sections and mode persists', async ({ page }) => {
    await page.goto('/benzknobz.html');

    const basicButton = page.getByRole('button', { name: 'Configure', exact: true });
    const advancedButton = page.getByRole('button', { name: 'Lab', exact: true });

    await expect(basicButton).toHaveAttribute('aria-pressed', 'true');
    await expect(page.locator('#performer-panel')).toBeHidden();
    await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Direct USB');
    await expect(page.locator('#connection-banner')).not.toContainText('Bridge');
    await expect
      .poll(async () => page.evaluate(() => window.__MN42_RUNTIME.getState().transportMode))
      .toBe('direct-webserial');
    await expect(page.locator('.runtime-lane-chip[data-runtime-lane="staged"]')).toBeHidden();
    await expect(page.locator('.runtime-lane-chip[data-runtime-lane="live"]')).toBeHidden();
    await expect(page.locator('.runtime-lane-chip[data-runtime-lane="browser"]')).toBeHidden();
    await expect(page.locator('.editor-tabbar')).toBeHidden();
    await expect(page.locator('#editor-panel')).toContainText('Shape this slot');
    await expect(page.locator('#check-compatibility')).toBeHidden();
    await expect(page.locator('#config-mode')).toBeHidden();
    await expect(page.locator('#rollback')).toHaveCount(0);
    await expect(page.locator('#profile-wizard')).toBeHidden();
    await expect(page.locator('#macro-card')).toBeHidden();
    await expect(page.locator('#scene-card')).toBeHidden();
    await expect(page.locator('#stage-power-summary')).toBeHidden();
    await expect(page.locator('#stage-scene-recall')).toBeHidden();
    await expect(page.locator('#stage-panic-help')).toBeHidden();
    await expect(page.locator('#usb-midi-toggle')).toBeHidden();
    await expect(page.locator('#simulator-toggle')).toBeHidden();
    await expect(page.locator('#device-monitor-section')).toBeHidden();
    await expect(page.locator('#slot-detail-panel')).toBeHidden();
    await expect(page.locator('#filter-settings')).toBeHidden();
    await expect(page.locator('#arg-settings')).toBeHidden();
    await expect(page.locator('#scope-panel')).toBeHidden();

    await advancedButton.click();
    await expect(advancedButton).toHaveAttribute('aria-pressed', 'true');
    await expect(page.locator('#performer-panel')).toBeHidden();
    await expect(page.locator('.runtime-lane-chip[data-runtime-lane="staged"]')).toBeVisible();
    await expect(page.getByRole('tab', { name: 'Console' })).toHaveAttribute('aria-selected', 'true');
    await expect(page.locator('#filter-settings')).toBeVisible();
    await expect(page.locator('#arg-settings')).toBeVisible();
    await expect(page.locator('#device-monitor-section')).toBeVisible();
    await page.getByRole('tab', { name: 'Console' }).press('End');
    await expect(page.getByRole('tab', { name: 'Scope' })).toBeFocused();
    await expect(page.locator('#scope-panel')).toBeVisible();

    await page.reload();
    await expect(advancedButton).toHaveAttribute('aria-pressed', 'true');
    await expect(page.locator('#filter-settings')).toBeVisible();
  });

  test('Configure empty state starts and connects the simulator', async ({ page }) => {
    await page.goto('/benzknobz.html');

    await expect(page.locator('#workspace-empty-state')).toBeVisible();
    await expect(page.locator('#empty-start-simulator')).toBeVisible();
    await expect(page.locator('#simulator-toggle')).toBeHidden();

    await page.locator('#empty-start-simulator').click();

    await expect(page.locator('#connection-pill')).toHaveText('Connected');
    await expect(page.locator('#workspace-empty-state')).toBeHidden();
    await expect(page.locator('.slot-editor')).toBeVisible();
    await expect
      .poll(async () => page.evaluate(() => window.__MN42_RUNTIME.getState().transportMode))
      .toBe('simulator');
  });

  test('basic mode still supports staged edits and apply', async ({ page }) => {
    await page.setViewportSize({ width: 1280, height: 633 });
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
    await expect(page.locator('#slot-detail-panel')).toBeHidden();
    await page.locator('#recovery-drawer > summary').click();
    await expect(page.locator('#export-preset')).toBeVisible();
    await expect(page.locator('#import-preset')).toBeVisible();
    await expect(page.locator('#profile-save')).toBeVisible();
    await expect(page.locator('#profile-load')).toBeVisible();
    await expect(page.locator('#profile-download')).toBeVisible();
    await expect(page.locator('#profile-upload')).toBeVisible();

    const ccInput = page.locator('.slot-editor label:has-text("CC number") input').first();
    await ccInput.fill('45');
    await ccInput.dispatchEvent('change');

    await expect(page.locator('#dirty-badge')).toBeVisible();
    await expect(page.locator('#change-bar')).toBeVisible();
    await expect(page.locator('#change-count')).toHaveText('● 1 staged');
    await expect(page.locator('#change-bar')).toHaveCSS('position', 'fixed');
    const initialBarBox = await page.locator('#change-bar').boundingBox();
    const initialViewport = page.viewportSize();
    expect(initialBarBox).not.toBeNull();
    expect(initialViewport).not.toBeNull();
    expect(initialBarBox.y).toBeGreaterThanOrEqual(0);
    expect(initialBarBox.y + initialBarBox.height).toBeLessThanOrEqual(initialViewport.height);
    await page.evaluate(() => window.scrollTo(0, document.body.scrollHeight));
    const barBox = await page.locator('#change-bar').boundingBox();
    const viewport = page.viewportSize();
    expect(barBox).not.toBeNull();
    expect(viewport).not.toBeNull();
    expect(barBox.y + barBox.height).toBeLessThanOrEqual(viewport.height);
    await page.locator('#change-review').click();
    await expect(page.locator('#change-review-dialog')).toHaveAttribute('open', '');
    await expect(page.locator('#change-review-output')).toContainText('1 staged change');
    await expect(page.locator('.change-review-group')).toHaveCount(1);
    await expect(page.locator('.change-review-values')).toContainText('Live');
    await expect(page.locator('.change-review-values')).toContainText('Staged');
    await page.locator('#change-review-close').click();
    const apply = page.locator('#apply');
    await expect(apply).toBeEnabled();
    await apply.click();
    await expect(page.locator('#status-label')).toHaveText('Synced', { timeout: 5000 });
    await expect(page.locator('#dirty-badge')).toBeHidden();
    await expect(page.locator('#change-bar')).toBeHidden();
  });
});
