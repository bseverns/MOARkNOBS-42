import { test, expect } from '@playwright/test';

test('app telemetry chunks are correctly merged by traceId with delayed dispatch', async ({
  page
}) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    window.testTelemetryFrames = [];
    window.__MN42_TEST_HOOKS = {
      mutateTransport(transport) {
        transport.protocol = 'native';
        let resolver = null;
        const queue = [];
        transport.writeLine = async (line) => {
          if (line === 'HELLO') queue.push(JSON.stringify({ hello: 'mn42' }));
          if (line === 'GET_MANIFEST') queue.push(JSON.stringify({ device_name: 'test' }));
          if (line === 'GET_SCHEMA') queue.push(JSON.stringify({}));
          if (line === 'GET_CONFIG') queue.push(JSON.stringify({}));
          if (resolver && queue.length) {
            const pending = resolver;
            resolver = null;
            pending(queue.shift());
          }
        };
        transport.nextLine = async () => {
          if (queue.length) return queue.shift();
          return new Promise((r) => {
            resolver = r;
          });
        };
        window.__pushLine = (line) => {
          if (resolver) {
            const pending = resolver;
            resolver = null;
            pending(line);
          } else {
            queue.push(line);
          }
        };
      }
    };
  });

  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: /simulator/i }).click();
  await page.getByRole('button', { name: 'Connect' }).click();

  await page.evaluate(() => {
    window.__MN42_RUNTIME.on('telemetry', (frame) => {
      window.testTelemetryFrames.push(frame);
    });
  });

  // Inject fragmented payloads atomically to beat Playwright's 10-15ms network latency per evaluate call
  await page.evaluate(() => {
    window.__pushLine(
      JSON.stringify({
        type: 'telemetry',
        traceId: 'fw-123',
        scope: 'state_slots',
        slots: [1, 2, 3],
        currentSlot: 0
      })
    );
    window.__pushLine(
      JSON.stringify({
        type: 'telemetry',
        traceId: 'fw-123',
        scope: 'state_args_0_13',
        slotArgs: Array.from({ length: 14 }, (_, i) => ({ index: i, enabled: 1 }))
      })
    );
    window.__pushLine(
      JSON.stringify({
        type: 'telemetry',
        traceId: 'fw-123',
        scope: 'state_args_14_27',
        slotArgs: Array.from({ length: 14 }, (_, i) => ({ index: i + 14, enabled: 1 }))
      })
    );
    window.__pushLine(
      JSON.stringify({
        type: 'telemetry',
        traceId: 'fw-123',
        scope: 'state_args_28_41',
        slotArgs: Array.from({ length: 14 }, (_, i) => ({ index: i + 28, enabled: 1 }))
      })
    );
    window.__pushLine(
      JSON.stringify({
        type: 'telemetry',
        traceId: 'fw-123',
        scope: 'state_diagnostics',
        diagnostics: { overruns: 0 }
      })
    );
  });

  // Wait for the native 50ms setTimeout to flush it reliably
  await page.waitForTimeout(150);

  let frames = await page.evaluate(() => window.testTelemetryFrames);
  // It should fire once due to the timeout.
  expect(frames.length).toBe(1);
  const payload = frames[0];

  expect(payload.slots).toEqual([1, 2, 3]);
  expect(payload.slotArgs).toHaveLength(42);
  expect(payload.slotArgs[0].index).toBe(0);
  expect(payload.slotArgs[14].index).toBe(14);
  expect(payload.slotArgs[28].index).toBe(28);
  expect(payload.diagnostics.overruns).toBe(0);

  // If we shift traceId mid-flight, it immediately flushes old cache
  // First clear our test log
  await page.evaluate(() => {
    window.testTelemetryFrames = [];
  });

  await page.evaluate(() => {
    window.__pushLine(JSON.stringify({ type: 'telemetry', traceId: 'fw-first', slots: [9] }));
    window.__pushLine(JSON.stringify({ type: 'telemetry', traceId: 'fw-first', envelopes: [8] }));

    // Send mismatched traceId synchronously!
    window.__pushLine(JSON.stringify({ type: 'telemetry', traceId: 'fw-next', lfos: [7] }));
  });

  // Wait for the async transport loop inside runtime.js to actually consume those queue strings
  await page.waitForTimeout(20);

  // Notice we don't wait 150ms since the flush was synchronous on ID-mismatch.
  frames = await page.evaluate(() => window.testTelemetryFrames);

  // Expected timeline: fw-next fires flushTelemetry() immediately for fw-first.
  // Then the new setTimeout starts for fw-next.
  expect(frames.length).toBe(1); // the fw-first frame

  expect(frames[0].slots).toEqual([9]);
  expect(frames[0].envelopes).toEqual([8]);
  expect(frames[0].lfos).toBeUndefined(); // LFOs belongs to fw-next which is buffered
});
