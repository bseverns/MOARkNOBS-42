import { test, expect } from '@playwright/test';

test.describe('Stage mode', () => {
  test('renders without editor and lab panels', async ({ page }) => {
    await page.goto('/?mode=stage');

    await expect(page.locator('#performer-panel [data-ui-mode-btn="stage"]')).toHaveAttribute(
      'aria-pressed',
      'true'
    );
    await expect(page.locator('#performer-panel')).toBeVisible();
    await expect(page.locator('#performer-panel [data-ui-mode-btn="basic"]')).toBeVisible();
    await expect(page.locator('#performer-panel [data-ui-mode-btn="advanced"]')).toBeVisible();
    await expect(page.locator('#editor-panel')).toBeHidden();
    await expect(page.locator('#power-safety-pill')).toBeHidden();
    await expect(page.locator('#filter-settings')).toBeHidden();
    await expect(page.locator('#arg-settings')).toBeHidden();
    await expect(page.locator('#led-settings')).toBeHidden();
    await expect(page.locator('#device-monitor-section')).toBeHidden();
    await expect(page.locator('#midi-panel')).toBeHidden();
    await expect(page.locator('#scope-panel')).toBeHidden();
    await expect(page.locator('.debug-log-bridge')).toBeHidden();
    await expect(page.locator('#simulator-toggle')).toBeHidden();
    await expect(page.locator('#import-preset')).toBeHidden();
    await expect(page.locator('#export-preset')).toBeHidden();
    await expect(page.locator('#preset-picker')).toBeHidden();
    await expect(page.locator('#apply-save-profile')).toBeHidden();
    await expect(page.locator('#status')).toBeVisible();
  });

  test('shows simulator manifest power fields in the performer panel', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    });
    await page.goto('/?mode=stage');

    await page.locator('#stage-connect').click();

    await expect(page.locator('#connection-pill')).toHaveText('Connected');
    await expect(page.locator('#stage-device-name')).toHaveText('MOARkNOBS-42');
    await expect(page.locator('#stage-fw-version')).toHaveText('sim-fw');
    await expect(page.locator('#stage-power-summary')).toContainText('POWER_CHOKED_V1');
    await expect(page.locator('#stage-power-summary')).toContainText('LED cap: 26/255');
    await expect(page.locator('#stage-power-summary')).toContainText('Rail: UNVERIFIED');
    await expect(page.locator('#stage-slots .stage-slot-cell')).toHaveCount(42);
    await expect(page.locator('#stage-envelopes .meter')).toHaveCount(6);
  });

  test('switching from Stage to Advanced restores bench tools', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    });
    await page.goto('/?mode=stage');
    await page.locator('#stage-connect').click();
    await expect(page.locator('#connection-pill')).toHaveText('Connected');

    await page.locator('#performer-panel [data-ui-mode-btn="advanced"]').click();

    await expect(page.locator('#performer-panel')).toBeVisible();
    await expect(page.locator('#live-panel')).toBeVisible();
    await expect(page.locator('#editor-panel')).toBeVisible();
    await expect(page.locator('#filter-settings')).toBeVisible();
    await expect(page.locator('#arg-settings')).toBeVisible();
    await expect(page.locator('#led-settings')).toBeVisible();
    await expect(page.locator('#device-monitor-section')).toBeVisible();
    await expect(page.locator('#simulator-toggle')).toBeVisible();

    const liveBox = await page.locator('#live-panel').boundingBox();
    const editorBox = await page.locator('#editor-panel').boundingBox();
    const utilityBox = await page.locator('#connect-card').boundingBox();
    if (!liveBox || !editorBox || !utilityBox) {
      throw new Error('Advanced workbench columns did not render');
    }
    expect(liveBox.x).toBeLessThan(editorBox.x);
    expect(editorBox.x).toBeLessThan(utilityBox.x);
    expect(utilityBox.width).toBeGreaterThan(300);
  });

  test('switching from Stage to Basic restores the calm editor surface', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    });
    await page.goto('/?mode=stage');
    await page.locator('#stage-connect').click();
    await expect(page.locator('#connection-pill')).toHaveText('Connected');

    await page.locator('#performer-panel [data-ui-mode-btn="basic"]').click();

    await expect(page.locator('#performer-panel')).toBeVisible();
    await expect(page.locator('#stage-panel')).toBeVisible();
    await expect(page.locator('#editor-panel')).toBeVisible();
    await expect(page.locator('#filter-settings')).toBeHidden();
    await expect(page.locator('#arg-settings')).toBeHidden();
    await expect(page.locator('#scope-panel')).toBeHidden();
  });

  test('dirty staged state does not expose editing controls in Stage', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    });
    await page.goto('/?mode=stage');
    await page.locator('#stage-connect').click();
    await expect(page.locator('#connection-pill')).toHaveText('Connected');

    await page.evaluate(() => {
      window.__MN42_RUNTIME.stage((draft) => {
        draft.slots[0].data1 = 77;
        return draft;
      });
    });
    await page.waitForFunction(() => window.__MN42_RUNTIME?.getState?.().dirty === true);

    await expect(page.locator('#stage-dirty-state')).toHaveText('Dirty');
    await expect(page.locator('#dirty-badge')).toBeVisible();
    await expect(page.locator('#apply')).toBeHidden();
    await expect(page.locator('#rollback')).toBeHidden();
    await expect(page.locator('#editor-panel')).toBeHidden();
    await expect(page.locator('.slot-editor')).toBeHidden();
    await expect(page.locator('#led-settings')).toBeHidden();
    await expect(page.locator('#export-preset')).toBeHidden();
  });

  test('panic help only displays the hardware combo text', async ({ page }) => {
    await page.addInitScript(() => {
      window.__nativeWrites = [];
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
      window.__MN42_TEST_HOOKS = {
        mutateTransport(transport) {
          const originalWriteLine = transport.writeLine.bind(transport);
          transport.writeLine = async (line) => {
            window.__nativeWrites.push(String(line ?? '').trim());
            return originalWriteLine(line);
          };
        }
      };
    });
    await page.goto('/?mode=stage');
    await page.locator('#stage-connect').click();
    await expect(page.locator('#connection-pill')).toHaveText('Connected');
    const writesBefore = await page.evaluate(() => window.__nativeWrites.length);

    await page.locator('#stage-panic-help').click();

    await expect(page.locator('#status-label')).toHaveText('Panic baseline');
    await expect(page.locator('.status-message')).toContainText('Ctrl0 + Ctrl1 + Ctrl2');
    await expect.poll(() => page.evaluate(() => window.__nativeWrites.length)).toBe(writesBefore);
  });
});
