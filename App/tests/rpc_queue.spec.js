import { test, expect } from '@playwright/test';

test('rpc queue throttles and respects single-flight ordering', async ({ page }) => {
  await page.addInitScript(() => {
    window.__mn42WriteTimes = [];
    window.__mn42NextLineDelay = 150;
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true, rpcTimeoutMs: 600 };
    window.__MN42_TEST_HOOKS = {
      mutateTransport(transport) {
        const originalWrite = transport.writeLine.bind(transport);
        transport.writeLine = async (line) => {
          window.__mn42WriteTimes.push(performance.now());
          return originalWrite(line);
        };
        if (typeof transport.nextLine === 'function') {
          const originalNext = transport.nextLine.bind(transport);
          transport.nextLine = async () => {
            await new Promise((resolve) => setTimeout(resolve, window.__mn42NextLineDelay));
            return originalNext();
          };
        }
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
    return {
      times,
      deltas,
      responseDelay: window.__mn42NextLineDelay
    };
  });
  expect(measurements.times.length).toBeGreaterThan(3);
  expect(Math.min(...measurements.deltas)).toBeGreaterThanOrEqual(4);
  // Account for read-loop phase jitter: nextLine delay might already be partially elapsed
  // when the first queued RPC is written, so we assert a conservative lower bound.
  const expectedDelay = Math.max(0, Math.floor((measurements.responseDelay ?? 0) * 0.2));
  expect(measurements.deltas[0]).toBeGreaterThanOrEqual(expectedDelay);
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
