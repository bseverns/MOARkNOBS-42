import { test, expect } from '@playwright/test';

async function bootConfigureSimulator(page) {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'basic');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });
  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('.slot-editor')).toBeVisible();
}

test('slot mapping names data1 for its selected MIDI type and hides it when unused', async ({
  page
}) => {
  await bootConfigureSimulator(page);

  const editor = page.locator('.slot-editor');
  const type = editor.getByLabel('Knob -> MIDI message');
  const data1 = editor.locator('[data-device-config-path~="slots.*.data1"]');

  await expect(editor.getByLabel('CC number')).toBeVisible();
  await editor.getByLabel('CC number').fill('74');
  await editor.getByLabel('CC number').blur();
  await expect
    .poll(() => page.evaluate(() => window.__MN42_RUNTIME.getState().staged.slots[0].data1))
    .toBe(74);

  for (const [value, label] of [
    ['Note', 'Note number'],
    ['ProgramChange', 'Program number'],
    ['NRPN', 'NRPN parameter'],
    ['RPN', 'RPN parameter']
  ]) {
    await type.selectOption(value);
    await type.blur();
    await expect(data1).toBeVisible();
    await expect(data1.locator('.control-label')).toContainText(label);
    await expect(data1.locator('input')).toHaveValue('74');
  }

  for (const value of ['PitchBend', 'Aftertouch', 'ModWheel', 'OFF']) {
    await type.selectOption(value);
    await type.blur();
    await expect(data1).toHaveCount(0);
  }

  await type.selectOption('SysEx');
  await type.blur();
  await expect(data1).toHaveCount(0);
  await expect(
    editor.locator('[data-device-config-path~="slots.*.sysexTemplate"] input')
  ).toBeVisible();
  await expect(editor.getByText('CC/Note number', { exact: true })).toHaveCount(0);
});
