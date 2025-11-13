import { test, expect } from '@playwright/test';

const statusLabel = (page) => page.locator('#status-label');
const statusMessage = (page) => page.locator('#status .status-message');

test.describe('Simulator transport flows', () => {
  test('handshake, validation, rollback, and toggles stay honest', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { ackTimeoutMs: 100 };
      window.__MN42_TEST_HOOKS = {
        mutateTransport(transport) {
          window.__mn42Transport = transport;
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
      const transport = window.__mn42Transport;
      if (!transport) throw new Error('Transport hook missing');
      if (transport.__testWrapped) return;
      const originalNextLine = transport.nextLine.bind(transport);
      let sentBadAck = false;
      transport.__testWrapped = true;
      transport.nextLine = async () => {
        if (!sentBadAck) {
          sentBadAck = true;
          return JSON.stringify({ type: 'ack', checksum: 'mismatch' });
        }
        return originalNextLine();
      };
    });

    await applyButton.click();
    await expect(statusLabel(page)).toHaveText('Apply failed');
    await expect(statusMessage(page)).toContainText('Device failed to acknowledge apply');
    await expect(page.locator('#dirty-badge')).toBeHidden();
    await expect(page.locator('#diff-panel')).toBeHidden();

    await simulatorToggle.click();
    await expect(simulatorToggle).toHaveText('Start simulator');
  });
});
