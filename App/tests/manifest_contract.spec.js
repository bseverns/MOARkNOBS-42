import { test, expect } from '@playwright/test';
import { createLocalManifest } from '../manifest_contract.js';
import { performConnectionHandshake } from '../runtime/connection_handshake.js';

const FIRMWARE_CAPABILITY_KEYS = [
  'profile_save',
  'profile_load',
  'profile_reset',
  'macro_snapshot',
  'scenes',
  'arp_live',
  'arp_profile_assignments',
  'clock_live',
  'note_dynamics_live',
  'jitter_live',
  'usb_midi_toggle',
  'device_schema',
  'bulk_config',
  'one_shot_config_boot'
];

test('local manifest exposes every firmware capability gate fail-closed', () => {
  const manifest = createLocalManifest();

  expect(Object.keys(manifest.capabilities).sort()).toEqual([...FIRMWARE_CAPABILITY_KEYS].sort());
  for (const key of FIRMWARE_CAPABILITY_KEYS) {
    expect(manifest.capabilities[key]).toBe(false);
  }
});

test('handshake fallback preserves the local firmware capability shape', async () => {
  const localManifest = createLocalManifest({ argMethodCount: 14 });
  const emitted = [];
  const originalDebug = console.debug;
  console.debug = () => {};

  try {
    const handshake = await performConnectionHandshake({
      sendRpc: async ({ rpc }) => {
        if (rpc === 'hello') return { hello: 'mn42' };
        throw new Error('manifest unavailable');
      },
      emit: (type, payload) => emitted.push({ type, payload }),
      localManifest,
      localSlotMetaManager: { ensureCount: () => {} },
      argMethodCount: 14
    });

    expect(handshake.quality).toBe('fallback-manifest');
    expect(handshake.manifest.build_time).toBeNull();
    expect(handshake.manifest.capabilities).toEqual(localManifest.capabilities);
    expect(emitted.find((entry) => entry.type === 'manifest')?.payload.capabilities).toEqual(
      localManifest.capabilities
    );
  } finally {
    console.debug = originalDebug;
  }
});

test('simulator manifest advertises the firmware lanes it emulates', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  const manifest = await page.evaluate(() => window.__MN42_RUNTIME.getState().manifest);

  expect(Object.keys(manifest.capabilities).sort()).toEqual([...FIRMWARE_CAPABILITY_KEYS].sort());
  expect(manifest.capabilities).toMatchObject({
    profile_save: true,
    profile_load: true,
    profile_reset: true,
    macro_snapshot: true,
    scenes: true,
    arp_live: true,
    arp_profile_assignments: true,
    clock_live: true,
    note_dynamics_live: true,
    jitter_live: true,
    usb_midi_toggle: true,
    device_schema: true,
    bulk_config: true,
    one_shot_config_boot: false
  });
});
