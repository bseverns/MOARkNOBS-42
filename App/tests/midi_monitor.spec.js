import { test, expect } from '@playwright/test';

test('midi monitor logs and clock without jitter', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    class MidiPort extends EventTarget {
      constructor(params) {
        super();
        this.id = params.id;
        this.name = params.name;
        this.onmidimessage = null;
      }
      send(data) {
        window.__MN42_MIDI_MOCK?.sent?.push([...data]);
      }
    }
    const input = new MidiPort({ id: 'mock-in', name: 'Mock Input' });
    const output = new MidiPort({ id: 'mock-out', name: 'Mock Output' });
    const midiAccess = {
      inputs: new Map([['mock-in', input]]),
      outputs: new Map([['mock-out', output]]),
      onstatechange: null,
      addEventListener(type, listener) {
        if (type === 'statechange') this.onstatechange = listener;
      },
      removeEventListener() {}
    };
    navigator.requestMIDIAccess = () => Promise.resolve(midiAccess);
    window.__MN42_MIDI_MOCK = {
      input,
      output,
      sent: [],
      emitIncoming(data) {
        const event = new Event('midimessage');
        event.data = new Uint8Array(data);
        event.target = input;
        input.dispatchEvent(event);
        if (typeof input.onmidimessage === 'function') {
          input.onmidimessage(event);
        }
      }
    };
  });

  await page.goto('/');
  await page.waitForFunction(() => document.documentElement?.dataset?.mn42Booted === 'true');
  await page.locator('#cold-start-simulator').click();
  await page.locator('.workspace-tool-shell > summary').click();
  await page.getByRole('button', { name: 'MIDI Monitor', exact: true }).click();
  await expect(page.locator('#midi-panel')).toBeVisible();

  const toggle = page.locator('#midi-panel-toggle');
  await expect(toggle).toBeVisible();
  await toggle.click();
  await expect(toggle).toHaveAttribute('aria-expanded', 'true');

  await page.getByRole('button', { name: /Enable MIDI/i }).click();
  await expect(page.locator('#midi-status')).toHaveText(/MIDI access granted/i, { timeout: 5000 });

  await page.evaluate(() => window.__MN42_MIDI_MOCK?.emitIncoming([0x90, 0x40, 0x7f]));
  await expect(page.locator('#midi-log-body .midi-row[data-direction="in"]')).toHaveCount(1);

  const clockButton = page.locator('#midi-clock-toggle');
  await expect(clockButton).toBeEnabled();
  await clockButton.click();
  await expect(clockButton).toHaveText(/Stop clock/i);
  await page.waitForFunction(
    () => document.querySelectorAll('#midi-log-body .midi-row[data-direction="out"]').length > 0
  );
  const outRows = page.locator('#midi-log-body .midi-row[data-direction="out"]');
  expect(await outRows.count()).toBeGreaterThan(0);
  await clockButton.click();
  await expect(clockButton).toHaveText(/Start clock/i);
});

test('a queued MIDI repaint does not rebuild a closed monitor', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    class MidiPort extends EventTarget {
      constructor(params) {
        super();
        this.id = params.id;
        this.name = params.name;
        this.onmidimessage = null;
      }
      send() {}
    }
    const input = new MidiPort({ id: 'mock-in', name: 'Mock Input' });
    const output = new MidiPort({ id: 'mock-out', name: 'Mock Output' });
    const midiAccess = {
      inputs: new Map([['mock-in', input]]),
      outputs: new Map([['mock-out', output]]),
      addEventListener() {},
      removeEventListener() {}
    };
    navigator.requestMIDIAccess = () => Promise.resolve(midiAccess);
    window.__MN42_MIDI_MOCK = {
      emitIncoming(data) {
        const event = new Event('midimessage');
        event.data = new Uint8Array(data);
        input.dispatchEvent(event);
        input.onmidimessage?.(event);
      }
    };
  });
  await page.goto('/');
  await page.waitForFunction(() => document.documentElement?.dataset?.mn42Booted === 'true');
  await page.locator('#cold-start-simulator').click();
  await page.locator('.workspace-tool-shell > summary').click();
  await page.getByRole('button', { name: 'MIDI Monitor', exact: true }).click();
  await page.locator('#midi-panel-toggle').click();
  await page.getByRole('button', { name: /Enable MIDI/i }).click();
  await expect(page.locator('#midi-status')).toHaveText(/MIDI access granted/i, { timeout: 5000 });

  await page.evaluate(() => {
    window.__midiMutationCount = 0;
    window.__midiQueuedFrame = null;
    const body = document.querySelector('#midi-log-body');
    new MutationObserver((records) => { window.__midiMutationCount += records.length; })
      .observe(body, { childList: true, subtree: true, characterData: true });
    window.requestAnimationFrame = (callback) => {
      window.__midiQueuedFrame = callback;
      return 1;
    };
    window.__MN42_MIDI_MOCK?.emitIncoming([0x90, 0x41, 0x7f]);
  });
  await page.getByRole('button', { name: 'MIDI Monitor', exact: true }).click();
  await page.evaluate(() => window.__midiQueuedFrame?.(performance.now()));
  expect(await page.evaluate(() => window.__midiMutationCount)).toBe(0);
});
