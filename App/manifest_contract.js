export const MN42_DEVICE_NAME = 'MOARkNOBS-42';
export const MN42_SCHEMA_VERSION = 6;
export const MN42_SLOT_COUNT = 42;
export const MN42_POT_COUNT = 42;
export const MN42_ENVELOPE_COUNT = 6;
export const MN42_LED_COUNT = 51;

export function createLocalManifest({ uiVersion, argMethodCount }) {
  return {
    ui_version: uiVersion,
    device_name: MN42_DEVICE_NAME,
    schema_version: MN42_SCHEMA_VERSION,
    slot_count: MN42_SLOT_COUNT,
    pot_count: MN42_POT_COUNT,
    envelope_count: MN42_ENVELOPE_COUNT,
    arg_method_count: argMethodCount,
    led_count: MN42_LED_COUNT
  };
}
