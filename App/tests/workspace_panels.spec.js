import { test, expect } from '@playwright/test';

async function bootSimulator(page, mode = 'basic') {
  await page.addInitScript(({ mode }) => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', mode);
    window.__MN42_RUNTIME_OPTIONS = {
      useSimulator: true,
      simulatorFixture: 'demo',
      workspacePanels: 'minimal'
    };
  }, { mode });
  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');
}

test.describe('progressive workspace panels', () => {
  test.beforeEach(async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { workspacePanels: 'minimal' };
    });
  });

  test('Bench and Lab start with clean optional workspaces', async ({ page }) => {
    await page.goto('/benzknobz.html?mode=basic');

    await expect(page.getByRole('button', { name: 'Bench', exact: true })).toHaveAttribute(
      'aria-pressed',
      'true'
    );
    await expect(page.getByRole('button', { name: 'Configure', exact: true })).toHaveCount(0);
    await expect(page.locator('#workspace-tool-picker')).toBeVisible();
    await expect(page.locator('[data-workspace-panel]:visible')).toHaveCount(0);

    await page.getByRole('button', { name: 'Lab', exact: true }).click();
    await expect(page.locator('[data-workspace-panel]:visible')).toHaveCount(0);
    await expect(page.getByRole('group', { name: 'Lab Observe tools' })).toBeVisible();
    await expect(page.getByRole('button', { name: 'Scope', exact: true })).toHaveAttribute(
      'aria-pressed',
      'false'
    );
  });

  test('Lab tools toggle independently inside their existing groups', async ({ page }) => {
    await bootSimulator(page, 'advanced');

    await page.getByRole('button', { name: 'Scope', exact: true }).click();
    await page.getByRole('button', { name: 'Device Monitor', exact: true }).click();
    await expect(page.locator('#scope-panel')).toBeVisible();
    await expect(page.locator('#device-monitor-section')).toBeVisible();
    await expect(page.locator('#scope-panel')).toHaveAttribute('data-scope-rendering', 'true');

    await page.getByRole('button', { name: 'Scope', exact: true }).click();
    await expect(page.locator('#scope-panel')).toBeHidden();
    await expect(page.locator('#scope-panel')).toHaveAttribute('data-scope-rendering', 'false');
    await expect(page.locator('#device-monitor-section')).toBeVisible();
  });

  test('mode changes retain open tools that remain eligible', async ({ page }) => {
    await page.goto('/benzknobz.html?mode=basic');
    await page.getByRole('button', { name: 'Live Slots', exact: true }).click();

    await page.getByRole('button', { name: 'Lab', exact: true }).click();

    await expect(page.locator('#live-panel')).toBeVisible();
    await expect(page.getByRole('button', { name: 'Live Slots', exact: true })).toHaveAttribute(
      'aria-pressed',
      'true'
    );
  });

  test('an initial tool does not reopen after the operator closes it', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = {};
    });
    await page.goto('/benzknobz.html?mode=basic');
    const liveSlots = page.getByRole('button', { name: 'Live Slots', exact: true });
    await expect(liveSlots).toHaveAttribute('aria-pressed', 'true');
    await liveSlots.click();

    await page.getByRole('button', { name: 'Lab', exact: true }).click();
    await page.getByRole('button', { name: 'Bench', exact: true }).click();

    await expect(page.locator('#live-panel')).toBeHidden();
    await expect(liveSlots).toHaveAttribute('aria-pressed', 'false');
  });

  test('the picker remains an interactive scroll target', async ({ page }) => {
    await page.goto('/benzknobz.html?mode=advanced');
    await expect(page.locator('#workspace-tool-picker')).toHaveCSS('pointer-events', 'auto');
  });

  test('selecting a Bench slot opens and focuses its editor on demand', async ({ page }) => {
    await bootSimulator(page, 'basic');

    await page.getByRole('button', { name: 'Live Slots', exact: true }).click();
    await page.locator('#slots [data-index="4"]').click();

    await expect(page.locator('#editor-panel')).toBeVisible();
    await expect(page.locator('#editor-panel')).toContainText('Shape this slot');
    await expect(page.locator('#editor-panel')).toHaveAttribute('data-workspace-open', 'true');
    await expect(page.locator('#editor-panel h2')).toBeFocused();
  });

  test('dirty drafts survive an explicitly confirmed mode transition', async ({ page }) => {
    await bootSimulator(page, 'basic');
    await page.getByRole('button', { name: 'Selected Slot Editor', exact: true }).click();
    const ccInput = page.locator('.slot-editor label:has-text("CC number") input').first();
    await ccInput.fill('45');
    await ccInput.dispatchEvent('change');
    await expect(page.locator('#dirty-badge')).toBeVisible();

    page.once('dialog', async (dialog) => {
      expect(dialog.message()).toContain('staged changes remain');
      await dialog.accept();
    });
    await page.getByRole('button', { name: 'Lab', exact: true }).click();

    await expect(page.getByRole('button', { name: 'Lab', exact: true })).toHaveAttribute(
      'aria-pressed',
      'true'
    );
    await expect(page.locator('#dirty-badge')).toBeVisible();
    expect(await page.evaluate(() => window.__MN42_RUNTIME.getState().staged.slots[0].data1)).toBe(45);
  });

  test('panel and mode navigation never writes to the device', async ({ page }) => {
    await page.addInitScript(() => {
      window.localStorage?.clear?.();
      window.__workspaceWrites = [];
      window.__MN42_RUNTIME_OPTIONS = {
      useSimulator: true,
      simulatorFixture: 'demo',
      workspacePanels: 'minimal'
    };
      window.__MN42_TEST_HOOKS = {
        mutateTransport(transport) {
          const writeLine = transport.writeLine.bind(transport);
          transport.writeLine = async (line) => {
            window.__workspaceWrites.push(String(line ?? '').trim());
            return writeLine(line);
          };
        }
      };
    });
    await page.goto('/benzknobz.html?mode=basic');
    await page.getByRole('button', { name: 'Connect' }).click();
    await expect(page.locator('#connection-pill')).toContainText('Connected');
    await page.waitForTimeout(250);
    const before = await page.evaluate(() => window.__workspaceWrites.length);

    await page.getByRole('button', { name: 'Live Slots', exact: true }).click();
    await page.getByRole('button', { name: 'Lab', exact: true }).click();
    await page.getByRole('button', { name: 'Scope', exact: true }).click();
    await page.getByRole('button', { name: 'Evidence Diff', exact: true }).click();
    await page.waitForTimeout(250);

    expect(await page.evaluate(() => window.__workspaceWrites.length)).toBe(before);
  });

  test('deep links open only the requested allowed panel', async ({ page }) => {
    await page.goto('/benzknobz.html?mode=advanced&panel=scope');

    await expect(page.locator('#scope-panel')).toBeVisible();
    await expect(page.getByRole('button', { name: 'Scope', exact: true })).toHaveAttribute(
      'aria-pressed',
      'true'
    );
    await expect(page.locator('[data-workspace-panel]:visible')).toHaveCount(1);
  });
});
