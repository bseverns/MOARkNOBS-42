import { test, expect } from '@playwright/test';

test.describe('UI mode', () => {
  test('connected Bench and Lab modes preserve explicit workspace ownership', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    });
    await page.goto('/benzknobz.html');
    await page.locator('#cold-start-simulator').click();
    await expect(page.locator('#connection-pill')).toHaveText('Connected');

    const benchButton = page.getByRole('button', { name: 'Bench', exact: true });
    const labButton = page.getByRole('button', { name: 'Lab', exact: true });
    await expect(benchButton).toHaveAttribute('aria-pressed', 'true');
    await expect(page.locator('#editor-panel')).toBeHidden();
    await page.locator('.workspace-tool-shell > summary').click();
    await page.getByRole('button', { name: 'Selected Slot Editor', exact: true }).click();
    await expect(page.locator('#editor-panel')).toBeVisible();

    await labButton.click();
    await expect(labButton).toHaveAttribute('aria-pressed', 'true');
    await page.getByRole('button', { name: 'EF Controls', exact: true }).click();
    await expect(page.locator('#filter-settings')).toBeVisible();

    await page.reload();
    await expect(labButton).toHaveAttribute('aria-pressed', 'true');
  });

  test('cold start hides device inventory until an explicit simulator choice', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { workspacePanels: 'minimal' };
    });
    await page.goto('/benzknobz.html');

    await expect(page.locator('#cold-start')).toBeVisible();
    await expect(page.locator('#workspace-empty-state')).toBeHidden();
    await expect(page.locator('.workspace-tool-shell')).toBeHidden();
    await expect(page.locator('#cold-start-simulator')).toBeVisible();
    await page.locator('.cold-recovery > summary').click();
    await page.getByRole('button', { name: 'Open recovery tools' }).click();
    await expect(page.locator('#recovery-drawer')).toBeVisible();
    await expect(page.locator('#import-preset')).toBeVisible();

    await page.getByRole('button', { name: 'Start simulator' }).click();

    await expect(page.locator('#connection-pill')).toHaveText('Connected');
    await expect(page.locator('#cold-start')).toBeHidden();
    await expect(page.locator('#workspace-empty-state')).toBeVisible();
    await expect(page.locator('.workspace-tool-shell')).toBeVisible();
    await expect(page.locator('.workspace-tool-shell')).not.toHaveAttribute('open', '');
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

    await page.locator('#cold-start-simulator').click();
    await expect(page.locator('#connection-banner')).not.toContainText('Bridge');
    await expect
      .poll(async () => page.evaluate(() => window.__MN42_RUNTIME.getState().transportMode))
      .toBe('simulator');
    await page.locator('#recovery-drawer > summary').click();
    await page.locator('#load-demo-rig').click();
    await page.locator('.workspace-tool-shell > summary').click();
    await page.getByRole('button', { name: 'Selected Slot Editor', exact: true }).click();
    await expect(page.locator('#slot-detail-panel')).toBeHidden();
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
