import { test, expect } from '@playwright/test';

async function simulatorState(page) {
  return page.evaluate(async () => {
    const runtime = window.__MN42_RUNTIME;
    return {
      state: runtime.getState(),
      profile: await runtime.sendRpc({ rpc: 'get_profile', slot: 0 }),
      matrix: await runtime.sendRpc({ rpc: 'get_mod_matrix' })
    };
  });
}

test('Start Simulator opens firmware defaults and Load Demo Rig names its illustrative fixture', async ({
  page
}) => {
  await page.goto('/benzknobz.html');
  await page.waitForFunction(() => document.documentElement.dataset.mn42Booted === 'true');

  await page.locator('#empty-start-simulator').click();
  await expect(page.locator('#connection-pill')).toHaveText('Connected');
  await expect(page.locator('#simulator-fixture-card')).toBeVisible();
  await expect(page.locator('#simulator-fixture-title')).toHaveText('Firmware defaults');

  const canonical = await simulatorState(page);
  expect(canonical.state.simulatorFixture).toBe('canonical');
  expect(canonical.state.live.filter).toMatchObject({ freq: 20, q: 1 });
  expect(canonical.state.live.slots).toHaveLength(42);
  canonical.state.live.slots.forEach((slot) => {
    expect(slot).toMatchObject({
      type: 'OFF',
      midiChannel: 1,
      data1: 0,
      efIndex: -1,
      active: false,
      arg: { enabled: false }
    });
    expect(slot.lfo).toEqual([
      { enabled: false, mode: 4, amount: 0 },
      { enabled: false, mode: 4, amount: 0 }
    ]);
  });
  expect(canonical.profile).toMatchObject({ active_profile: 0, routes: [] });
  expect(canonical.profile.lfos.map((lfo) => lfo.depth)).toEqual([0, 0]);
  expect(canonical.matrix.routes).toEqual([]);
  expect(canonical.matrix.conflicts).toEqual([]);

  await page.locator('#load-demo-rig').click();
  await expect(page.locator('#simulator-fixture-title')).toHaveText('Demo Rig');

  const demo = await simulatorState(page);
  expect(demo.state.simulatorFixture).toBe('demo');
  expect(demo.state.live.slots.some((slot) => slot.active)).toBe(true);
  expect(new Set(demo.state.live.slots.map((slot) => slot.midiChannel)).size).toBeGreaterThan(1);
  expect(demo.state.live.slots.some((slot) => slot.arg.enabled)).toBe(true);
  expect(demo.profile.routes).toHaveLength(2);
  expect(demo.profile.lfos.map((lfo) => lfo.depth)).toEqual([1, 0.5]);

  await page.locator('#load-firmware-defaults').click();
  await expect(page.locator('#simulator-fixture-title')).toHaveText('Firmware defaults');
  await expect
    .poll(async () => page.evaluate(() => window.__MN42_RUNTIME.getState().simulatorFixture))
    .toBe('canonical');
});

test('Load Demo Rig can be chosen as the first hardware-free session', async ({ page }) => {
  await page.goto('/benzknobz.html');
  await page.waitForFunction(() => document.documentElement.dataset.mn42Booted === 'true');

  await page.locator('#empty-load-demo-rig').click();
  await expect
    .poll(async () => page.evaluate(() => window.__MN42_RUNTIME.getState().simulatorFixture))
    .toBe('demo');
  await expect(page.locator('#connection-pill')).toHaveText('Connected');
  await expect(page.locator('#simulator-fixture-title')).toHaveText('Demo Rig');
  await expect
    .poll(async () => page.evaluate(() => window.__MN42_RUNTIME.getState().simulatorFixture))
    .toBe('demo');
});
