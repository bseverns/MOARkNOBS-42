import { test, expect } from '@playwright/test';
import { promises as fs } from 'node:fs';
import path from 'node:path';

async function openMode(page, mode) {
  await page.addInitScript((nextMode) => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', nextMode);
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  }, mode);
  await page.setViewportSize({ width: 1440, height: 1000 });
  await page.goto(`/?mode=${mode}`);
  const connectButton =
    mode === 'stage'
      ? page.locator('#stage-connect')
      : page.getByRole('button', { name: 'Connect' });
  await connectButton.click();
  await expect(page.locator('#connection-pill')).toHaveText('Connected');
}

async function openModeAtViewport(page, mode, viewport) {
  await page.addInitScript((nextMode) => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', nextMode);
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  }, mode);
  await page.setViewportSize(viewport);
  await page.goto(`/?mode=${mode}`);
  await (mode === 'stage'
    ? page.locator('#stage-connect')
    : page.getByRole('button', { name: 'Connect' })).click();
  await expect(page.locator('#connection-pill')).toHaveText('Connected');
}

test.describe('Mode screenshots', () => {
  for (const mode of ['stage', 'basic', 'advanced']) {
    test(`${mode} mode screenshot artifact`, async ({ page }) => {
      await openMode(page, mode);
      const screenshotDir = path.resolve('test-results/screenshots');
      await fs.mkdir(screenshotDir, { recursive: true });
      await page.screenshot({
        path: path.join(screenshotDir, `${mode}-mode.png`),
        fullPage: true
      });
    });
  }

  test('stage Motion drawer screenshot artifact', async ({ page }) => {
    await openMode(page, 'stage');
    await page.locator('#stage-motion > summary').click();
    await expect(page.locator('#stage-motion-panel')).toBeVisible();
    const overflow = await page.evaluate(() => document.documentElement.scrollWidth - window.innerWidth);
    expect(overflow).toBeLessThanOrEqual(1);
    const screenshotDir = path.resolve('test-results/screenshots');
    await fs.mkdir(screenshotDir, { recursive: true });
    await page.screenshot({
      path: path.join(screenshotDir, 'stage-motion-open.png'),
      fullPage: true
    });
  });

  const viewportMatrix = [
    { name: 'phone', width: 390, height: 844, mode: 'stage' },
    { name: 'tablet-portrait', width: 768, height: 1024, mode: 'stage' },
    { name: 'tablet-landscape', width: 1024, height: 768, mode: 'basic' },
    { name: 'laptop', width: 1366, height: 768, mode: 'basic' },
    { name: 'standard', width: 1440, height: 1000, mode: 'basic' },
    { name: 'lab-hd', width: 1920, height: 1080, mode: 'advanced' },
    { name: 'workstation', width: 2560, height: 1080, mode: 'advanced' }
  ];

  for (const viewport of viewportMatrix) {
    test(`${viewport.name} viewport renders without overflow`, async ({ page }) => {
      const pageErrors = [];
      page.on('pageerror', (error) => pageErrors.push(error.message));
      await openModeAtViewport(page, viewport.mode, viewport);

      const overflow = await page.evaluate(() => document.documentElement.scrollWidth - window.innerWidth);
      expect(overflow).toBeLessThanOrEqual(1);
      expect(pageErrors).toEqual([]);

      await page.locator('.connection-details > summary').click();
      const detailBox = await page.locator('.connection-detail-card').boundingBox();
      expect(detailBox).not.toBeNull();
      expect(detailBox.x).toBeGreaterThanOrEqual(0);
      expect(detailBox.x + detailBox.width).toBeLessThanOrEqual(viewport.width);

      if (viewport.width <= 520) {
        const bannerBox = await page.locator('#connection-banner').boundingBox();
        expect(bannerBox).not.toBeNull();
        const overlapsBanner =
          detailBox.x < bannerBox.x + bannerBox.width &&
          detailBox.x + detailBox.width > bannerBox.x &&
          detailBox.y < bannerBox.y + bannerBox.height &&
          detailBox.y + detailBox.height > bannerBox.y;
        expect(overlapsBanner).toBe(false);
      }

      if (viewport.mode === 'stage') {
        const summaryColumns = await page.locator('.performer-summary-grid').evaluate(
          (element) => getComputedStyle(element).gridTemplateColumns.split(' ').length
        );
        expect(summaryColumns).toBe(viewport.width <= 520 ? 1 : 2);
        const slotColumns = await page.locator('.stage-slot-grid').evaluate(
          (element) => getComputedStyle(element).gridTemplateColumns.split(' ').length
        );
        expect(slotColumns).toBe(viewport.width <= 760 ? 3 : 6);
      }

      const screenshotDir = path.resolve('test-results/screenshots/viewports');
      await fs.mkdir(screenshotDir, { recursive: true });
      await page.screenshot({
        path: path.join(screenshotDir, `${viewport.name}-${viewport.mode}.png`),
        fullPage: true
      });
    });
  }
});
