import { test, expect } from '@playwright/test';

const statusLabel = (page) => page.locator('#status-label');
const statusMessage = (page) => page.locator('#status .status-message');

test.describe('Simulator transport flows', () => {
  test('handshake, validation, rollback, and toggles stay honest', async ({ page }) => {
    await page.addInitScript(() => {
      window.__mn42TestState = { poisonAck: false, ackPoisoned: false };
      window.__MN42_RUNTIME_OPTIONS = {
        // Give the simulated firmware plenty of time to echo the checksum ACK.
        // The simulator throttles writes to keep parity with real hardware, so
        // we trade a slightly longer wait for deterministic assertions instead
        // of chasing timeout-induced flakes.
        ackTimeoutMs: 2000
      };
      window.__MN42_TEST_HOOKS = {
        mutateTransport(transport) {
          window.__mn42Transport = transport;
        },
        interceptAck(ack) {
          const state = window.__mn42TestState;
          if (state?.poisonAck && !state.ackPoisoned) {
            state.ackPoisoned = true;
            return { ...ack, checksum: 'mismatch' };
          }
          return ack;
        }
      };
    });

    await page.goto('/benzknobz.html');

    const simulatorToggle = page.getByRole('button', { name: /simulator/i });
    await expect(simulatorToggle).toHaveText('Start simulator');
    await simulatorToggle.click();
    await expect(simulatorToggle).toHaveText('Stop simulator');

    await page.getByRole('button', { name: 'Connect' }).click();
    await expect(page.locator('#connection-pill')).toHaveText(/Connected •/);
    await expect(statusLabel(page)).toHaveText('Connected');

    const argB = page.locator('#arg-b');
    await argB.fill('9');
    await argB.blur();
    const applyButton = page.getByRole('button', { name: 'Apply' });
    await expect(applyButton).toBeEnabled();

    await applyButton.click();
    await expect(statusLabel(page)).toHaveText('Apply failed');
    await expect(statusMessage(page)).toContainText('Schema validation failed');
    await expect(page.locator('#diff-output')).toContainText('maximum 5');

    await argB.fill('3');
    await argB.blur();

    await page.evaluate(() => {
      const slider = document.querySelector('#led-grid input[type="range"]');
      if (!slider) throw new Error('LED slider missing');
      slider.value = '200';
      slider.dispatchEvent(new Event('input', { bubbles: true }));
      slider.dispatchEvent(new Event('change', { bubbles: true }));
    });
    await expect(applyButton).toBeEnabled();

    await page.evaluate(() => {
      const state = window.__mn42TestState;
      if (!state) throw new Error('Test state missing');
      state.poisonAck = true;
      state.ackPoisoned = false;
    });

    await applyButton.click();
    await expect(statusLabel(page)).toHaveText('Apply failed');
    await expect(statusMessage(page)).toContainText('Device failed to acknowledge apply');
    await expect(page.locator('#dirty-badge')).toBeHidden();
    await expect(page.locator('#diff-panel')).toBeHidden();

    await page.evaluate(() => {
      const state = window.__mn42TestState;
      if (state) state.poisonAck = false;
    });

    await simulatorToggle.click();
    await expect(simulatorToggle).toHaveText('Start simulator');
  });

  test('migration dialog and diff/rollback flows stay wired', async ({ page }) => {
    await page.addInitScript(() => {
      window.__mn42TestState = { poisonAck: false, ackPoisoned: false };
      window.__MN42_RUNTIME_OPTIONS = {
        // Let adapter rehearsal finish chunking before the checksum gate slams.
        // Staging + migration adds a few extra message hops; a generous timeout
        // keeps the assertions focused on diff/rollback wiring instead of ACK
        // races in headless CI.
        ackTimeoutMs: 2000,
        migrations: {
          '3->4': (config) => config
        }
      };
      window.__MN42_TEST_HOOKS = {
        mutateTransport(transport) {
          window.__mn42Transport = transport;
          let manifestPatched = false;
          const originalNextLine = transport.nextLine.bind(transport);
          transport.nextLine = async () => {
            const line = await originalNextLine();
            if (!manifestPatched && line) {
              try {
                const payload = JSON.parse(line);
                if (payload && typeof payload === 'object' && payload.schema_version === 4) {
                  manifestPatched = true;
                  payload.schema_version = 3;
                  return JSON.stringify(payload);
                }
              } catch (_) {
                // ignore malformed JSON and fall back to the original line
              }
            }
            return line;
          };
        },
        interceptAck: (ack) => ack
      };
    });

    await page.goto('/benzknobz.html');

    const simulatorToggle = page.getByRole('button', { name: /simulator/i });
    await simulatorToggle.click();
    await page.getByRole('button', { name: 'Connect' }).click();

    const migrationDialog = page.locator('#migration-dialog');
    await expect(migrationDialog).toBeVisible();
    const migrationPreview = page.locator('#migration-preview');
    await expect(migrationPreview).toContainText('Firmware schema 3 vs UI 4');
    await expect(migrationPreview).toContainText('An adapter is available');

    await page.getByRole('button', { name: 'Apply adapter' }).click();
    await expect(migrationDialog).toBeHidden();
    await expect(statusLabel(page)).toHaveText(/Migration ready/i);

    const argA = page.locator('#arg-a');
    await argA.fill('2');
    await argA.blur();

    await expect(page.locator('#dirty-badge')).toBeVisible();
    await expect(page.locator('#diff-panel')).toBeVisible();

    const rollbackButton = page.getByRole('button', { name: 'Rollback' });
    await rollbackButton.click();
    await expect(statusLabel(page)).toHaveText(/Rolled back/i);
    await expect(page.locator('#dirty-badge')).toBeHidden();
    await expect(page.locator('#diff-panel')).toBeHidden();

    await argA.fill('1');
    await argA.blur();

    const applyButton = page.getByRole('button', { name: 'Apply' });
    await expect(applyButton).toBeEnabled();

    await applyButton.click();
    await expect(statusLabel(page)).toHaveText(/Synced/i);
    await expect(page.locator('#dirty-badge')).toBeHidden();
    await expect(page.locator('#diff-panel')).toBeHidden();
    await expect(applyButton).toBeDisabled();

    await simulatorToggle.click();
    await expect(simulatorToggle).toHaveText('Start simulator');
  });
});
