import { test, expect } from '@playwright/test';

test.describe('Power safety summary', () => {
  async function renderPowerSummary(page, manifest) {
    return page.evaluate(async (payload) => {
      const { renderPowerSummary } = await import('/views/controllers/power_safety_summary.js');
      const container = document.createElement('div');
      renderPowerSummary(container, payload);
      return Array.from(container.querySelectorAll('span')).map((node) => ({
        text: node.textContent,
        railState: node.dataset.railState ?? ''
      }));
    }, manifest);
  }

  test('renders choked board power profile and unverified rail state', async ({ page }) => {
    await page.goto('/');

    const rows = await renderPowerSummary(page, {
      power_profile: 'POWER_CHOKED_V1',
      led_brightness_cap: 26,
      rail_topology_verified: false
    });

    expect(rows.map((row) => row.text)).toEqual([
      'Power: POWER_CHOKED_V1',
      'LED cap: 26/255',
      'Rail: UNVERIFIED'
    ]);
    expect(rows[2].railState).toBe('unverified');
  });

  test('renders verified rail state', async ({ page }) => {
    await page.goto('/');

    const rows = await renderPowerSummary(page, {
      power_profile: 'SPLIT_RAIL_REWORK',
      led_brightness_cap: 255,
      rail_topology_verified: true
    });

    expect(rows.map((row) => row.text)).toEqual([
      'Power: SPLIT_RAIL_REWORK',
      'LED cap: 255/255',
      'Rail: VERIFIED'
    ]);
    expect(rows[2].railState).toBe('verified');
  });

  test('renders missing fields as unknown or dash placeholders', async ({ page }) => {
    await page.goto('/');

    const rows = await renderPowerSummary(page, {});

    expect(rows.map((row) => row.text)).toEqual(['Power: -', 'LED cap: -', 'Rail: UNKNOWN']);
    expect(rows[2].railState).toBe('unknown');
  });
});
