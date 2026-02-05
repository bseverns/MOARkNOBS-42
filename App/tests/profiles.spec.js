import { test, expect } from '@playwright/test';
import { promises as fs } from 'fs';
import path from 'path';

test.describe('Profiles toolbar', () => {
  async function bootWithSimulator(page) {
    await page.addInitScript(() => {
      window.localStorage?.clear?.();
      window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    });
    await page.goto('/benzknobz.html');
    const simulatorToggle = page.getByRole('button', { name: /simulator/i });
    await simulatorToggle.click();
    await page.getByRole('button', { name: 'Connect' }).click();
  }

  test('saving then loading a slot restores staged edits', async ({ page }) => {
    await bootWithSimulator(page);

    const freqInput = page.locator('[data-schema-target="filter"] input[type="number"]').first();
    await freqInput.waitFor({ state: 'visible' });
    await freqInput.fill('123');
    await freqInput.dispatchEvent('change');
    await expect(page.locator('#dirty-badge')).toBeVisible();

    const applyButton = page.getByRole('button', { name: 'Apply' });
    await applyButton.click();
    await expect(page.locator('#status-label')).toHaveText('Synced', { timeout: 5000 });
    await expect(page.locator('#dirty-badge')).toBeHidden();

    await page.getByRole('button', { name: 'Save profile', exact: true }).click();
    await expect(page.locator('#status-label')).toHaveText('Profile saved', { timeout: 5000 });

    await freqInput.fill('200');
    await freqInput.dispatchEvent('change');
    await page.getByRole('button', { name: 'Switch profile', exact: true }).click();
    await expect(page.locator('#status-label')).toHaveText('Profile switched', { timeout: 5000 });
    await expect(freqInput).toHaveValue('123');
    await expect(page.locator('#dirty-badge')).toBeHidden();
  });

  test('save profile auto-applies dirty staged edits', async ({ page }) => {
    await bootWithSimulator(page);

    const freqInput = page.locator('[data-schema-target="filter"] input[type="number"]').first();
    await freqInput.fill('444');
    await freqInput.dispatchEvent('change');
    await expect(page.locator('#dirty-badge')).toBeVisible();

    await page.getByRole('button', { name: 'Save profile', exact: true }).click();
    await expect(page.locator('#status-label')).toHaveText('Profile saved', { timeout: 5000 });
    await expect(page.locator('#dirty-badge')).toBeHidden();
  });

  test('guided wizard switches, applies, and saves target slot', async ({ page }) => {
    await bootWithSimulator(page);

    await page.selectOption('#profile-wizard-target', '2');
    await page.getByRole('button', { name: '1. Switch target', exact: true }).click();
    await expect(page.locator('#status-label')).toHaveText('Profile switched', { timeout: 5000 });
    await expect(page.locator('#profile-slot-status')).toContainText('Slot C');

    const freqInput = page.locator('[data-schema-target="filter"] input[type="number"]').first();
    await freqInput.fill('321');
    await freqInput.dispatchEvent('change');
    await expect(page.locator('#dirty-badge')).toBeVisible();

    await page.getByRole('button', { name: '2. Sync edits', exact: true }).click();
    await expect(page.locator('#status-label')).toHaveText('Synced', { timeout: 5000 });
    await expect(page.locator('#dirty-badge')).toBeHidden();

    await page.getByRole('button', { name: '3. Save profile', exact: true }).click();
    await expect(page.locator('#status-label')).toHaveText('Profile saved', { timeout: 5000 });
  });

  test('uploading a backup stages the imported profile', async ({ page }) => {
    await bootWithSimulator(page);

    const staged = await page.evaluate(() => window.__MN42_RUNTIME?.getState().staged);
    const updated = staged && typeof staged === 'object' ? JSON.parse(JSON.stringify(staged)) : {};
    updated.slots = Array.isArray(updated.slots) ? updated.slots.slice() : [];
    updated.slots[0] = {
      ...(updated.slots[0] ?? {}),
      data1: 12,
      midiChannel: 6
    };

    const targetValue = 56;
    updated.slots[0].data1 = targetValue;

    const filePath = test.info().outputPath('profile-import.json');
    await fs.mkdir(path.dirname(filePath), { recursive: true });
    await fs.writeFile(filePath, JSON.stringify({ slot: 0, config: updated }, null, 2));

    const [chooser] = await Promise.all([
      page.waitForEvent('filechooser'),
      page.getByRole('button', { name: 'Upload profile' }).click()
    ]);
    await chooser.setFiles(filePath);

    await page.waitForFunction(
      (value) => {
        const runtime = window.__MN42_RUNTIME;
        if (!runtime) return false;
        const stagedData = runtime.getState().staged;
        return stagedData?.slots?.[0]?.data1 === value;
      },
      targetValue
    );
    await expect(page.locator('#status-label')).toHaveText('Profile imported', { timeout: 5000 });
  });
});
