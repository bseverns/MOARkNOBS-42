import { test, expect } from '@playwright/test';

async function openIncomingMidi(page) {
  await page.getByRole('tab', { name: 'Incoming MIDI', exact: true }).click();
  const inputSection = page.locator('[data-schema-target="midiInputBindings"]');
  await expect(inputSection).toBeVisible();
  return inputSection;
}

test('MIDI input bindings can be added, edited, and removed as staged profile config', async ({
  page
}) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/');
  await page.getByRole('button', { name: 'Connect' }).click();

  const inputSection = await openIncomingMidi(page);
  await inputSection.locator('.schema-array-add').click();

  const binding = inputSection.locator('details').first();
  await expect(binding).toHaveCount(1);
  await expect(binding).toHaveAttribute('open', '');
  await binding.getByLabel('CC number').fill('74');
  await binding.getByLabel('Control target').selectOption('arp.swing');
  await expect(binding.locator('.midi-binding-route-text')).toHaveText(
    'DIN + USB · Ch 1 · CC 74 → Arpeggiator · Swing'
  );

  await expect(page.locator('#dirty-badge')).toBeVisible();
  await expect
    .poll(() =>
      page.evaluate(() => window.__MN42_RUNTIME.getState().staged.midiInputBindings?.[0])
    )
    .toMatchObject({
      source: { port: 'any', type: 'cc7', channel: 1, number: 74 },
      destination: 'arp.swing',
      mode: 'absolute',
      outputRange: [0, 127],
      pickup: 'soft'
    });

  await inputSection.getByRole('button', { name: 'Remove MIDI input route 1' }).click();
  await expect(inputSection.locator('details')).toHaveCount(0);
  await expect(inputSection.locator('.midi-binding-empty')).toContainText(
    'No bindings in this profile'
  );
});

test('removing a MIDI input binding cancels its pending field edit', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/');
  await page.getByRole('button', { name: 'Connect' }).click();

  const inputSection = await openIncomingMidi(page);
  await inputSection.locator('.schema-array-add').click();
  const binding = inputSection.locator('details').first();

  // Dispatch the edit and removal in one browser task so the 150 ms field
  // debounce cannot settle before the rendered control is destroyed.
  await binding.evaluate((detail) => {
    const destination = detail.querySelector('[data-config-path$=".destination"]');
    destination.value = 'arp.swing';
    destination.dispatchEvent(new Event('input', { bubbles: true }));
    detail.querySelector('.schema-array-remove').click();
  });

  await page.waitForTimeout(250);
  await expect(inputSection.locator('details')).toHaveCount(0);
  await expect
    .poll(() => page.evaluate(() => window.__MN42_RUNTIME.getState().staged.midiInputBindings))
    .toEqual([]);
});

test('rerendering an array preserves a synchronously staged select edit', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/');
  await page.getByRole('button', { name: 'Connect' }).click();

  const inputSection = await openIncomingMidi(page);
  await inputSection.locator('.schema-array-add').click();
  const binding = inputSection.locator('details').first();

  await binding.evaluate((detail) => {
    const port = detail.querySelector('[data-config-path$=".source.port"]');
    port.value = 'usb';
    port.dispatchEvent(new Event('input', { bubbles: true }));
    detail.closest('[data-schema-target]').querySelector('.schema-array-add').click();
  });

  await expect
    .poll(() => page.evaluate(() => window.__MN42_RUNTIME.getState().staged.midiInputBindings))
    .toMatchObject([{ source: { port: 'usb' } }, { source: { port: 'any' } }]);
});

test('MIDI input binding editor presents grouped targets and keeps output range valid', async ({
  page
}) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/');
  await page.getByRole('button', { name: 'Connect' }).click();

  const inputSection = await openIncomingMidi(page);
  await expect(inputSection.locator('.midi-binding-count')).toHaveText('0 of 16 bindings');
  await inputSection.getByRole('button', { name: 'Add binding' }).click();

  const binding = inputSection.locator('.midi-binding-card').first();
  await expect(binding.getByRole('group', { name: 'Incoming CC' })).toBeVisible();
  await expect(binding.getByRole('group', { name: 'Destination' })).toBeVisible();
  await expect(binding.getByRole('group', { name: 'Response' })).toBeVisible();
  await expect(binding.getByLabel('Message')).toHaveCount(0);
  await expect(binding.getByLabel('Control target').locator('optgroup')).toHaveCount(2);

  const takeover = binding.getByLabel('Takeover');
  await expect(takeover).toBeEnabled();
  await expect(takeover).toHaveValue('soft');
  await binding.getByLabel('Interaction').selectOption('momentary');
  await expect(takeover).toBeDisabled();
  await expect(takeover).toHaveValue('soft');
  await binding.getByLabel('Interaction').selectOption('absolute');
  await expect(takeover).toBeEnabled();
  await expect(takeover).toHaveValue('soft');

  await binding.getByLabel('Output maximum').fill('40');
  await binding.getByLabel('Output minimum').fill('72');
  await expect(binding.getByLabel('Output maximum')).toHaveValue('72');
  await expect
    .poll(() =>
      page.evaluate(
        () => window.__MN42_RUNTIME.getState().staged.midiInputBindings?.[0]?.outputRange
      )
    )
    .toEqual([72, 72]);

  const paths = await binding.locator('[data-device-config-path]').evaluateAll((elements) =>
    elements.map((element) => element.dataset.deviceConfigPath.replace(/\.0\./, '.*.')).sort()
  );
  expect(paths).toEqual([
    'midiInputBindings.*.destination',
    'midiInputBindings.*.mode',
    'midiInputBindings.*.outputRange.0',
    'midiInputBindings.*.outputRange.1',
    'midiInputBindings.*.pickup',
    'midiInputBindings.*.source.channel',
    'midiInputBindings.*.source.number',
    'midiInputBindings.*.source.port',
    'midiInputBindings.*.source.type'
  ]);
});

test('MIDI input binding groups contain their controls at the desktop review width', async ({
  page
}) => {
  await page.setViewportSize({ width: 1356, height: 1000 });
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/');
  await page.getByRole('button', { name: 'Connect' }).click();
  const inputSection = await openIncomingMidi(page);
  await inputSection.getByRole('button', { name: 'Add binding' }).click();

  const containment = await inputSection.locator('.midi-binding-card').evaluate((card) => {
    const source = card.querySelector('.midi-binding-source');
    const destination = card.querySelector('.midi-binding-destination');
    const sourceGrid = card.querySelector('.midi-binding-source-grid');
    const sourceRect = source.getBoundingClientRect();
    const destinationRect = destination.getBoundingClientRect();
    const fields = [...sourceGrid.querySelectorAll('.midi-binding-field')];
    return {
      groupsDoNotOverlap:
        sourceRect.right <= destinationRect.left ||
        destinationRect.right <= sourceRect.left ||
        sourceRect.bottom <= destinationRect.top ||
        destinationRect.bottom <= sourceRect.top,
      copyFits: source.querySelector('.midi-binding-group-copy').scrollWidth <= source.clientWidth,
      gridFits: sourceGrid.scrollWidth <= sourceGrid.clientWidth,
      fieldsFit: fields.every((field) => {
        const fieldRect = field.getBoundingClientRect();
        return fieldRect.left >= sourceRect.left && fieldRect.right <= sourceRect.right;
      })
    };
  });

  expect(containment).toEqual({
    groupsDoNotOverlap: true,
    copyFits: true,
    gridFits: true,
    fieldsFit: true
  });
});

test('MIDI input binding editor collapses to one usable column on a phone', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/');
  await page.getByRole('button', { name: 'Connect' }).click();
  const inputSection = await openIncomingMidi(page);
  await inputSection.getByRole('button', { name: 'Add binding' }).click();

  const sourceGrid = inputSection.locator('.midi-binding-source-grid');
  await expect(sourceGrid).toBeVisible();
  expect(
    await sourceGrid.evaluate(
      (element) => getComputedStyle(element).gridTemplateColumns.split(' ').length
    )
  ).toBe(1);
  expect(
    await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)
  ).toBe(true);
});

test('Incoming MIDI is profile-owned and not part of the selected-slot workspace', async ({
  page
}) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/');
  await page.getByRole('button', { name: 'Connect' }).click();

  const incomingPanel = page.locator('[data-performance-panel="incoming"]');
  await expect(incomingPanel).toBeHidden();
  await openIncomingMidi(page);
  await expect(incomingPanel).toBeVisible();
  await expect(incomingPanel.locator('h3')).toHaveText('Incoming MIDI');
  await expect(page.locator('#form #midi-input-form')).toHaveCount(0);
  await expect(page.locator('#profile-performance-panels #midi-input-form')).toHaveCount(1);
});
