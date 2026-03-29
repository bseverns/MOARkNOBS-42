export const MN42_DEVICE_NAME = 'MOARkNOBS-42';
export const MN42_SCHEMA_VERSION = 6;
export const MN42_SLOT_COUNT = 42;
export const MN42_POT_COUNT = 42;
export const MN42_ENVELOPE_COUNT = 6;
export const MN42_LED_COUNT = 51;

const DEFAULT_CAPABILITIES = {
  profile_save: false,
  profile_load: false,
  profile_reset: false,
  macro_snapshot: false,
  scenes: false
};

// Build the browser's fallback manifest so the UI still knows slot counts,
// schema version, and capability defaults before a real device handshakes.
export function createLocalManifest({ uiVersion, argMethodCount, capabilities } = {}) {
  return {
    ui_version: uiVersion,
    device_name: MN42_DEVICE_NAME,
    schema_version: MN42_SCHEMA_VERSION,
    slot_count: MN42_SLOT_COUNT,
    pot_count: MN42_POT_COUNT,
    envelope_count: MN42_ENVELOPE_COUNT,
    arg_method_count: argMethodCount,
    led_count: MN42_LED_COUNT,
    capabilities: {
      ...DEFAULT_CAPABILITIES,
      ...(capabilities && typeof capabilities === 'object' ? capabilities : {})
    }
  };
}
