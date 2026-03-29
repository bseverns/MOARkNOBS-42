// Compatibility stub: the lightweight validator already bakes in the few
// format checks this app needs, so callers can keep the AJV-style setup flow.
export default function addFormats() {
  // The bundled validator handles the limited formats in config_schema.json directly.
  return undefined;
}
