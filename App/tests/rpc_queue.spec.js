import { test, expect } from '@playwright/test';

test('rpc queue throttles and honors timeouts', async ({ page }) => {
  await page.addInitScript(() => {
    window.__mn42WriteTimes = [];
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true, rpcTimeoutMs: 600 };
    window.__MN42_TEST_HOOKS = {
      mutateTransport(transport) {
        const original = transport.writeLine.bind(transport);
        transport.writeLine = async (line) => {
          window.__mn42WriteTimes.push(performance.now());
          return original(line);
        };
      }
    };
  });

  await page.goto('/benzknobz.html');
  const simulatorToggle = page.getByRole('button', { name: /simulator/i });
  await simulatorToggle.click();
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  await page.evaluate(() => {
    window.__mn42WriteTimes.length = 0;
  });

  await page.evaluate(async () => {
    const runtime = window.__MN42_RUNTIME;
    await Promise.all([
      runtime.sendRpc({ rpc: 'hello' }),
      runtime.sendRpc({ rpc: 'hello' }),
      runtime.sendRpc({ rpc: 'hello' }),
      runtime.sendRpc({ rpc: 'hello' })
    ]);
  });

  const measurements = await page.evaluate(() => {
    const deltas = [];
    const times = window.__mn42WriteTimes;
    for (let i = 1; i < times.length; i += 1) {
      deltas.push(times[i] - times[i - 1]);
    }
    return { times, deltas };
  });
  expect(measurements.times.length).toBeGreaterThan(3);
  expect(Math.min(...measurements.deltas)).toBeGreaterThanOrEqual(4);

  const timeoutMessage = await page.evaluate(async () => {
    try {
      await window.__MN42_RUNTIME.sendRpc({ rpc: 'hang' });
      return null;
    } catch (err) {
      return err?.message ?? String(err);
    }
  });
  expect(timeoutMessage).toMatch(/RPC timeout/);
});
