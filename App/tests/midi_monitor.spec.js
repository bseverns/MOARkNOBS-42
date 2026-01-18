import { test, expect } from '@playwright/test';

test('midi monitor logs and clock without jitter', async ({ page }) => {
  await page.addInitScript(() => {
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

  await page.goto('/benzknobz.html');

  const toggle = page.locator('#midi-panel-toggle');
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
    () => document.querySelectorAll('#midi-log-body .midi-row[data-direction="out"]').length > 0,
  );
  const outRows = page.locator('#midi-log-body .midi-row[data-direction="out"]');
  expect(await outRows.count()).toBeGreaterThan(0);
  await clockButton.click();
  await expect(clockButton).toHaveText(/Start clock/i);
});
