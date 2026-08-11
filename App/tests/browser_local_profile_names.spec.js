import { test, expect } from '@playwright/test';

test.beforeEach(async ({ page }) => {
  await page.addInitScript(() => {
    if (window.sessionStorage?.getItem('profile-name-test-ready')) return;
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.sessionStorage?.setItem('profile-name-test-ready', 'true');
  });
  await page.goto('/benzknobz.html');
});

test('browser-local profile names persist and never dirty firmware config', async ({ page }) => {
  await page.locator('#recovery-drawer > summary').click();
  const input = page.locator('#profile-name');

  await input.fill('  Ambient   Set  ');
  await input.blur();

  await expect(input).toHaveValue('Ambient Set');
  await expect(page.locator('#profile-slot-status')).toContainText('Ambient Set • Profile A');
  await expect(page.locator('#header-profile-status')).toHaveText('Ambient Set');
  await expect(page.locator('#profile-save')).toHaveText('Save to Ambient Set (Profile A)');
  await expect(page.locator('#apply')).toBeDisabled();

  const localState = await page.evaluate(() => ({
    names: JSON.parse(window.localStorage.getItem('moarknobs:profile-names')),
    dirty: window.__MN42_RUNTIME.getState().dirty,
    diff: window.__MN42_RUNTIME.diff()
  }));
  expect(localState).toEqual({ names: ['Ambient Set', '', '', ''], dirty: false, diff: [] });

  await page.getByRole('button', { name: 'Stage', exact: true }).click();
  await expect(page.locator('#stage-profile-select option[value="0"]')).toHaveText(
    'Ambient Set · Profile A'
  );
  await expect(page.locator('#stage-profile-load')).toHaveText(
    'Recall Ambient Set (Profile A) now'
  );

  await page.reload();
  await expect(page.locator('#header-profile-status')).toHaveText('Ambient Set');
  await expect(page.locator('#stage-profile-select option[value="0"]')).toHaveText(
    'Ambient Set · Profile A'
  );
});

test('each profile slot keeps an independent local name', async ({ page }) => {
  await page.locator('#recovery-drawer > summary').click();
  await page.locator('[data-profile-slot="1"]').click();
  await page.locator('#profile-name').fill('Bass Rig');
  await page.locator('#profile-name').blur();

  await expect(page.locator('#profile-slot-status')).toContainText('Bass Rig • Profile B');
  await page.locator('[data-profile-slot="0"]').click();
  await expect(page.locator('#profile-name')).toHaveValue('');
  await page.locator('[data-profile-slot="1"]').click();
  await expect(page.locator('#profile-name')).toHaveValue('Bass Rig');
});
