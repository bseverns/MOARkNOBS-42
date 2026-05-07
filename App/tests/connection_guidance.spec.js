import { test, expect } from '@playwright/test';
import { promises as fs } from 'fs';
import path from 'path';

function buildImportConfig() {
  return {
    slots: Array.from({ length: 42 }, (_, index) => ({
      type: 'CC',
      midiChannel: 1,
      data1: index === 0 ? 99 : index % 128,
      efIndex: -1,
      ef: {
        index: -1,
        filter_index: 0,
        filter_name: 'LINEAR',
        frequency: 1000,
        q: 0.707,
        oversample: 4,
        smoothing: 0.2,
        baseline: 0,
        gain: 1
      },
      active: true,
      arg: {
        enabled: false,
        method: 0,
        method_name: 'PLUS',
        sourceA: 0,
        sourceB: 1
      }
    })),
    efSlots: Array.from({ length: 6 }, () => ({ slots: [] })),
    filter: { type: 'LINEAR', freq: 500, q: 1 },
    arg: { method: 'PLUS', a: 0, b: 1, enable: false },
    led: { brightness: 26, hex: '#112233', rgb: { r: 17, g: 34, b: 51 }, mode: 'STATIC' }
  };
}

test('compatibility check reports when Web Serial is unavailable', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    Object.defineProperty(navigator, 'serial', {
      configurable: true,
      value: undefined
    });
  });

  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: 'Check compatibility' }).click();

  await expect(page.locator('#status-label')).toHaveText('Web Serial unavailable');
  await expect(page.locator('.status-message')).toContainText('cannot access the Web Serial API');
});

test('connect distinguishes a cancelled device picker from other errors', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    Object.defineProperty(navigator, 'serial', {
      configurable: true,
      value: {
        requestPort: async () => {
          throw new DOMException('The chooser was dismissed.', 'NotFoundError');
        }
      }
    });
  });

  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: 'Connect' }).click();

  await expect(page.locator('#status-label')).toHaveText('No device selected');
  await expect(page.locator('.status-message')).toContainText('browser picker');
});

test('connect ignores malformed remembered port filters so the picker can open', async ({
  page
}) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem('moarknobs:last-port', JSON.stringify({}));
    window.__requestPortOptions = null;
    Object.defineProperty(navigator, 'serial', {
      configurable: true,
      value: {
        requestPort: async (options) => {
          window.__requestPortOptions = options;
          throw new DOMException('The chooser was dismissed.', 'NotFoundError');
        }
      }
    });
  });

  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: 'Connect' }).click();

  const options = await page.evaluate(() => window.__requestPortOptions);
  expect(options).toEqual({});
  await expect(page.locator('#status-label')).toHaveText('No device selected');
});

test('config JSON can be imported before connecting and exported afterward', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    Object.defineProperty(navigator, 'serial', {
      configurable: true,
      value: undefined
    });
  });

  await page.goto('/benzknobz.html');

  const importButton = page.getByRole('button', { name: 'Import config JSON' });
  const exportButton = page.getByRole('button', { name: 'Export config JSON' });
  await expect(importButton).toBeEnabled();
  await expect(exportButton).toBeDisabled();

  const filePath = test.info().outputPath('config-import.json');
  await fs.mkdir(path.dirname(filePath), { recursive: true });
  await fs.writeFile(filePath, JSON.stringify(buildImportConfig(), null, 2));

  const [chooser] = await Promise.all([page.waitForEvent('filechooser'), importButton.click()]);
  await chooser.setFiles(filePath);

  await page.waitForFunction(() => {
    const runtime = window.__MN42_RUNTIME;
    return runtime?.getState?.().staged?.slots?.[0]?.data1 === 99;
  });

  await expect(page.locator('#status-label')).toHaveText('Config imported');
  await expect(exportButton).toBeEnabled();

  const [download] = await Promise.all([page.waitForEvent('download'), exportButton.click()]);
  expect(download.suggestedFilename()).toMatch(/^moarknobz-config-.*\.json$/);
});
