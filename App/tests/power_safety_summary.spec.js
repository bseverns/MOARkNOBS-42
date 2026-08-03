import { test, expect } from '@playwright/test';

test.describe('Power safety summary', () => {
  async function renderPowerSummary(page, manifest, connected = true) {
    return page.evaluate(async ({ payload, connected: isConnected }) => {
      const { renderPowerSummary } = await import('/views/controllers/power_safety_summary.js');
      const container = document.createElement('div');
      renderPowerSummary(container, payload, { connected: isConnected });
      return {
        rows: Array.from(container.querySelectorAll('span')).map((node) => ({
          text: node.textContent,
          railState: node.dataset.railState ?? ''
        })),
        warning: container.querySelector('.power-safety-warning')?.textContent ?? '',
        powerAvailable: container.dataset.powerAvailable,
        powerWarning: container.dataset.powerWarning
      };
    }, { payload: manifest, connected });
  }

  test('renders choked board power profile and unverified rail state', async ({ page }) => {
    await page.goto('/');

    const rendered = await renderPowerSummary(page, {
      power_profile: 'POWER_CHOKED_V1',
      led_brightness_cap: 26,
      rail_topology_verified: false
    });

    expect(rendered.rows.map((row) => row.text)).toEqual([
      'Power: POWER_CHOKED_V1',
      'LED cap: 26/255',
      'Rail: UNVERIFIED'
    ]);
    expect(rendered.rows[2].railState).toBe('unverified');
    expect(rendered.warning).toContain('Power-limited hardware reported');
    expect(rendered.powerWarning).toBe('true');
  });

  test('renders split-rail and verified-rail claims as neutral metadata', async ({ page }) => {
    await page.goto('/');

    const rendered = await renderPowerSummary(page, {
      power_profile: 'SPLIT_RAIL_REWORK',
      led_brightness_cap: 255,
      rail_topology_verified: true
    });

    expect(rendered.rows.map((row) => row.text)).toEqual([
      'Power: SPLIT_RAIL_REWORK',
      'LED cap: 255/255',
      'Rail: VERIFIED'
    ]);
    expect(rendered.rows[2].railState).toBe('verified');
    expect(rendered.warning).toBe('');
    expect(rendered.powerWarning).toBe('false');
  });

  test('renders missing connected fields as neutral unavailable metadata', async ({ page }) => {
    await page.goto('/');

    const rendered = await renderPowerSummary(page, {});

    expect(rendered.rows.map((row) => row.text)).toEqual([
      'Power: unavailable',
      'LED cap: unavailable',
      'Rail: UNKNOWN'
    ]);
    expect(rendered.rows[2].railState).toBe('unknown');
    expect(rendered.warning).toBe('');
  });

  test('does not present fallback manifest data as attached-device truth', async ({ page }) => {
    await page.goto('/');

    const rendered = await renderPowerSummary(page, {
      power_profile: 'POWER_CHOKED_V1',
      led_brightness_cap: 26,
      rail_topology_verified: false
    }, false);

    expect(rendered.rows.map((row) => row.text)).toEqual(['Power status unavailable']);
    expect(rendered.powerAvailable).toBe('false');
    expect(rendered.powerWarning).toBe('false');
    expect(rendered.warning).toBe('');
  });
});
