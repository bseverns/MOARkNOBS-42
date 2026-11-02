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
  await page.locator('#cold-start-simulator').click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');
  await openWorkspaceTools(page);
}

async function openWorkspaceTools(page) {
  const tools = page.locator('.workspace-tool-shell');
  if (!(await tools.evaluate((element) => element.open))) {
    await tools.locator('summary').click();
  }
}

test.describe('progressive workspace panels', () => {
  test.beforeEach(async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { workspacePanels: 'minimal' };
    });
  });

  test('Bench and Lab start with clean optional workspaces', async ({ page }) => {
    await bootSimulator(page, 'basic');
    await page.locator('.workspace-tool-shell > summary').click();

    await expect(page.getByRole('button', { name: 'Bench', exact: true })).toHaveAttribute(
      'aria-pressed',
      'true'
    );
    await expect(page.getByRole('button', { name: 'Configure', exact: true })).toHaveCount(0);
    await expect(page.locator('#workspace-tool-picker')).toBeHidden();
    await expect(page.locator('#live-panel')).toBeVisible();
    await expect(page.locator('#editor-panel')).toBeVisible();
    await expect(page.locator('#selected-slot-signal')).toBeVisible();

    await page.getByRole('button', { name: 'Lab', exact: true }).click();
    await expect(page.locator('[data-workspace-panel]:visible')).toHaveCount(2);
    await openWorkspaceTools(page);
    await expect(page.getByRole('tablist', { name: 'Lab Bench workspaces' })).toHaveCount(0);
    await expect(page.getByRole('group', { name: 'Lab Observe tools' })).toBeVisible();
    const observeLegend = page.locator('[data-workspace-tool-group="Lab Observe"] legend');
    await expect(observeLegend).toBeVisible();
    await expect(observeLegend).toHaveCSS('position', 'static');
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

  test('hidden Device Monitor retains data without rebuilding its DOM', async ({ page }) => {
    await bootSimulator(page, 'advanced');
    const monitor = page.locator('#device-monitor-section');
    await page.getByRole('button', { name: 'Device Monitor', exact: true }).click();
    await expect(monitor.locator('.monitor-row')).not.toHaveCount(0);
    await page.getByRole('button', { name: 'Device Monitor', exact: true }).click();

    await page.evaluate(() => {
      window.__deviceMonitorMutations = 0;
      const monitor = document.querySelector('#device-monitor');
      window.__deviceMonitorObserver = new MutationObserver((records) => {
        window.__deviceMonitorMutations += records.length;
      });
      window.__deviceMonitorObserver.observe(monitor, { childList: true, subtree: true, characterData: true });
    });
    await page.waitForTimeout(500);
    expect(await page.evaluate(() => window.__deviceMonitorMutations)).toBe(0);

    await page.getByRole('button', { name: 'Device Monitor', exact: true }).click();
    await expect(monitor.locator('.monitor-row')).not.toHaveCount(0);
    await page.evaluate(() => window.__deviceMonitorObserver.disconnect());
  });

  test('mode changes retain open tools that remain eligible', async ({ page }) => {
    await bootSimulator(page, 'basic');

    await page.getByRole('button', { name: 'Lab', exact: true }).click();

    await expect(page.locator('#live-panel')).toBeVisible();
    await expect(page.getByRole('button', { name: 'Live Slots', exact: true })).toHaveAttribute(
      'aria-pressed',
      'true'
    );
  });

  test('a tool does not reopen after the operator closes it', async ({ page }) => {
    await bootSimulator(page, 'basic');
    const liveSlots = page.getByRole('button', { name: 'Live Slots', exact: true });
    await expect(liveSlots).toHaveAttribute('aria-pressed', 'true');
    await liveSlots.click();

    await page.getByRole('button', { name: 'Lab', exact: true }).click();
    await page.getByRole('button', { name: 'Bench', exact: true }).click();

    await expect(page.locator('#live-panel')).toBeHidden();
    await expect(liveSlots).toHaveAttribute('aria-pressed', 'false');
  });

  test('the connected fallback appears only after the final Bench panel closes', async ({ page }) => {
    await bootSimulator(page, 'basic');
    const liveSlots = page.getByRole('button', { name: 'Live Slots', exact: true });
    const editor = page.getByRole('button', { name: 'Selected Slot Editor', exact: true });

    await liveSlots.click();
    await expect(page.locator('#workspace-empty-state')).toBeHidden();
    await editor.click();
    await expect(page.locator('#workspace-empty-state')).toBeVisible();
  });

  test('the picker remains an interactive touch target', async ({ page }) => {
    await bootSimulator(page, 'advanced');
    await expect(page.locator('#workspace-tool-picker')).toHaveCSS('pointer-events', 'auto');
    await expect(page.getByRole('button', { name: 'Scope', exact: true })).toHaveCSS('min-height', '44px');
  });

  test('selecting a Bench slot keeps its editor context available without stealing focus', async ({ page }) => {
    await bootSimulator(page, 'basic');

    await page.locator('#slots [data-index="4"]').click();

    await expect(page.locator('#editor-panel')).toBeVisible();
    await expect(page.locator('#editor-panel')).toContainText('Shape this slot');
    await expect(page.locator('#editor-panel')).toHaveAttribute('data-workspace-open', 'true');
    await expect(page.locator('#editor-panel h2')).not.toBeFocused();
  });

  test('slot grid retains keyboard focus across consecutive selection keys', async ({ page }) => {
    await bootSimulator(page, 'basic');
    const slots = page.locator('#slots');
    await slots.focus();
    await page.keyboard.press('ArrowRight');
    await page.keyboard.press('ArrowRight');
    await expect(slots).toBeFocused();
    await expect(slots).toHaveAttribute('aria-activedescendant', 'slot-grid-option-2');
    await expect(page.locator('#slot-grid-option-0')).toHaveAttribute('aria-pressed', 'false');
    await expect(page.locator('#slot-grid-option-2')).toHaveAttribute('aria-pressed', 'true');
    await page.keyboard.press('Enter');
    await expect(page.locator('#editor-panel')).toBeVisible();
  });

  test('dirty drafts survive a mode transition without confirmation', async ({ page }) => {
    await bootSimulator(page, 'basic');
    await page.evaluate(() => {
      window.__modeConfirmCalled = false;
      window.confirm = () => {
        window.__modeConfirmCalled = true;
        return false;
      };
    });
    await page.getByRole('button', { name: 'Lab', exact: true }).click();
    await page.locator('#recovery-drawer > summary').click();
    await page.locator('#load-demo-rig').click();
    await page.getByRole('button', { name: 'Bench', exact: true }).click();
    const ccInput = page.locator('.slot-editor label:has-text("CC number") input').first();
    await ccInput.fill('45');
    await ccInput.dispatchEvent('change');
    await expect(page.locator('#dirty-badge')).toBeVisible();

    await page.getByRole('button', { name: 'Lab', exact: true }).click();

    await expect(page.getByRole('button', { name: 'Lab', exact: true })).toHaveAttribute(
      'aria-pressed',
      'true'
    );
    await expect(page.locator('#dirty-badge')).toBeVisible();
    await expect(page.evaluate(() => window.__modeConfirmCalled)).resolves.toBe(false);
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
    await page.locator('#cold-start-simulator').click();
    await expect(page.locator('#connection-pill')).toContainText('Connected');
    await openWorkspaceTools(page);
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
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true, workspacePanels: 'minimal' };
    });
    await page.goto('/benzknobz.html?mode=advanced&panel=scope');
    await page.locator('#connect').click();
    await openWorkspaceTools(page);

    await expect(page.locator('#scope-panel')).toBeVisible();
    await expect(page.getByRole('button', { name: 'Scope', exact: true })).toHaveAttribute(
      'aria-pressed',
      'true'
    );
    await expect(page.locator('[data-workspace-panel]:visible')).toHaveCount(1);
  });

  test('legacy panels deep link opens exactly its requested eligible panels', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true, workspacePanels: 'minimal' };
    });
    await page.goto('/benzknobz.html?mode=advanced&panels=scope,device-monitor');
    await page.locator('#connect').click();
    await openWorkspaceTools(page);

    await expect(page.locator('#scope-panel')).toBeVisible();
    await expect(page.locator('#device-monitor-section')).toBeVisible();
    await expect(page.locator('[data-workspace-panel]:visible')).toHaveCount(2);
  });

  for (const [legacyId, tab] of [['arp-profile', 'arp'], ['lfo-profile', 'lfo'], ['incoming-midi', 'incoming']]) {
    test(`legacy ${legacyId} deep link opens its Profile Performance tab`, async ({ page }) => {
      await page.addInitScript(() => {
        window.__MN42_RUNTIME_OPTIONS = { useSimulator: true, workspacePanels: 'minimal' };
      });
      await page.goto(`/benzknobz.html?mode=advanced&panel=${legacyId}`);
      await page.locator('#connect').click();
      await openWorkspaceTools(page);

      await expect(page.getByRole('button', { name: 'Profile Performance', exact: true })).toHaveAttribute('aria-pressed', 'true');
      await expect(page.locator(`[data-performance-panel="${tab}"]`)).toBeVisible();
      await expect(page.locator('[data-workspace-panel]:visible')).toHaveCount(1);
      await expect(page).toHaveURL(/panel=profile-performance/);
    });
  }

  test('Workspace Tools supports keyboard disclosure and panel toggling', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true, workspacePanels: 'minimal' };
    });
    await page.goto('/benzknobz.html?mode=advanced');
    await page.locator('#cold-start-simulator').click();
    await expect(page.locator('#connection-pill')).toContainText('Connected');
    const summary = page.locator('.workspace-tool-shell > summary');
    await summary.press('Enter');
    await expect(page.locator('.workspace-tool-shell')).toHaveAttribute('open', '');

    const scope = page.getByRole('button', { name: 'Scope', exact: true });
    await scope.press('Enter');
    await expect(scope).toHaveAttribute('aria-pressed', 'true');
    await expect(page.locator('#scope-panel')).toBeVisible();
  });
});
