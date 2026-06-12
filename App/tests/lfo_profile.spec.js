import { test, expect } from '@playwright/test';

async function bootWithSimulator(page) {
  await page.waitForFunction(() => document.documentElement.dataset.mn42Booted === 'true');
  await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Simulator');
}

test('LFO edits expose a save action and persist through set_profile', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');
  await bootWithSimulator(page);
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  await page.locator('[data-utility-tab="lfo"]').click();
  await expect(page.locator('[data-utility-panel="lfo"]')).toBeVisible();
  await expect(page.locator('#lfo-status')).toContainText('2 LFOs');

  const save = page.locator('#lfo-save');
  await expect(save).toHaveText('Save slot LFOs');
  await expect(save).toBeDisabled();

  const firstDepth = page.locator('#lfo-editor .lfo-section').first().getByLabel('Depth');
  await firstDepth.fill('0.42');
  await firstDepth.dispatchEvent('change');

  await expect(page.locator('#lfo-status')).toContainText('edited locally');
  await expect(save).toHaveText('Push & save LFO changes');
  await expect(save).toBeEnabled();

  await save.click();
  await expect(page.locator('#lfo-status')).toContainText('saved');
  await expect(save).toHaveText('Save slot LFOs');
  await expect(save).toBeDisabled();

  const profile = await page.evaluate(() =>
    window.__MN42_RUNTIME.sendRpc({ rpc: 'get_profile', slot: 0 })
  );
  expect(profile.lfos[0].depth).toBe(0.42);
});

test('inactive slot LFO save keeps the edited slot visible', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');
  await bootWithSimulator(page);
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  await page.locator('#recovery-drawer').evaluate((drawer) => {
    drawer.open = true;
  });
  await page.locator('[data-profile-slot="1"]').click();
  await page.locator('[data-utility-tab="lfo"]').click();
  await expect(page.locator('[data-utility-panel="lfo"]')).toBeVisible();
  await expect(page.locator('#profile-slot-status')).toContainText('Slot B');
  await expect(page.locator('#profile-slot-status')).toContainText('board active A');

  const firstDepth = page.locator('#lfo-editor .lfo-section').first().getByLabel('Depth');
  await firstDepth.fill('0.37');
  await firstDepth.dispatchEvent('change');

  await page.locator('#lfo-save').click();
  await expect(page.locator('#lfo-status')).toContainText('Slot B saved');
  await expect(page.locator('#lfo-status')).toContainText('Slot A is active');
  await expect(page.locator('#profile-slot-status')).toContainText('Slot B');
  await expect(page.locator('#profile-slot-status')).toContainText('board active A');

  const slotA = await page.evaluate(() =>
    window.__MN42_RUNTIME.sendRpc({ rpc: 'get_profile', slot: 0 })
  );
  const slotB = await page.evaluate(() =>
    window.__MN42_RUNTIME.sendRpc({ rpc: 'get_profile', slot: 1 })
  );
  expect(slotA.lfos[0].depth).not.toBe(0.37);
  expect(slotB.lfos[0].depth).toBe(0.37);
});

test('active slot LFO save does not warn to switch to itself', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');
  await bootWithSimulator(page);
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  await page.evaluate(() => {
    const runtime = window.__MN42_RUNTIME;
    const originalSendRpc = runtime.sendRpc.bind(runtime);
    runtime.sendRpc = async (message, options) => {
      const response = await originalSendRpc(message, options);
      if (message?.rpc === 'set_profile') {
        return {
          ...response,
          profile: message.slot,
          active_profile: message.slot,
          active_applied: false
        };
      }
      return response;
    };
  });

  await page.locator('[data-utility-tab="lfo"]').click();
  await expect(page.locator('[data-utility-panel="lfo"]')).toBeVisible();

  const firstDepth = page.locator('#lfo-editor .lfo-section').first().getByLabel('Depth');
  await firstDepth.fill('0.33');
  await firstDepth.dispatchEvent('change');

  await page.locator('#lfo-save').click();
  await expect(page.locator('#lfo-status')).toContainText(
    'Slot A saved to the active board profile'
  );
  await expect(page.locator('#lfo-status')).not.toContainText('Switch to Slot A');
  await expect(page.locator('#profile-slot-status')).toContainText('Slot A');
  await expect(page.locator('#profile-slot-status')).toContainText('active on board');
});
