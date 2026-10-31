import { test, expect } from '@playwright/test';

async function bootProfileWorkspace(page) {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true, simulatorFixture: 'demo' };
  });
  await page.goto('/benzknobz.html');
  await page.locator('#cold-start-simulator').click();
  await expect(page.locator('#connection-pill')).toHaveText('Connected');
  await page.locator('.workspace-tool-shell > summary').click();
  await page.getByRole('button', { name: 'Profile Performance', exact: true }).click();
}

test('Profile Performance owns one workspace while its tabs select its contents', async ({ page }) => {
  await bootProfileWorkspace(page);
  const tool = page.getByRole('button', { name: 'Profile Performance', exact: true });
  const workspace = page.locator('#profile-performance-workspace');

  await expect(tool).toHaveAttribute('aria-pressed', 'true');
  await expect(workspace).toBeVisible();
  await expect(page.locator('[data-workspace-panel]:visible')).toHaveCount(1);

  for (const tab of ['arp', 'lfo', 'incoming']) {
    await page.locator(`[data-performance-tab="${tab}"]`).click();
    await expect(page.locator(`[data-performance-panel="${tab}"]`)).toBeVisible();
    await expect(tool).toHaveAttribute('aria-pressed', 'true');
    await expect(page.locator('[data-workspace-panel]:visible')).toHaveCount(1);
  }
  await expect(page).toHaveURL(/panel=profile-performance/);
});

test('dirty LFO draft survives reconnect and requires confirmation before profile switch', async ({ page }) => {
  await bootProfileWorkspace(page);
  await page.locator('[data-performance-tab="lfo"]').click();
  const depth = page.locator('#lfo-editor .lfo-section').first().getByLabel('Depth');
  await depth.fill('0.42');
  await depth.dispatchEvent('change');

  await page.evaluate(() => window.__MN42_RUNTIME.disconnect());
  await expect(depth).toHaveValue('0.42');
  await page.locator('#connect').click();
  await expect(page.locator('#connection-pill')).toHaveText('Connected');
  await expect(depth).toHaveValue('0.42');

  page.once('dialog', (dialog) => dialog.dismiss());
  await page.evaluate(() => document.querySelector('[data-profile-slot="1"]').click());
  await expect(depth).toHaveValue('0.42');
  await expect(page.locator('#profile-slot-status')).toContainText('Slot A');
});

test('dirty profile arp draft survives runtime error and reconnect', async ({ page }) => {
  await bootProfileWorkspace(page);
  await page.locator('[data-performance-tab="arp"]').click();
  await page.getByRole('button', { name: /Profile .* Defaults/ }).click();
  const pattern = page.locator('#arp-pattern-length');
  await pattern.fill('9');

  await page.evaluate(() => window.__MN42_RUNTIME.disconnect());
  await expect(pattern).toHaveValue('9');
  await page.locator('#connect').click();
  await expect(page.locator('#connection-pill')).toHaveText('Connected');
  await expect(pattern).toHaveValue('9');
});
