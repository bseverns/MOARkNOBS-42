export const CONFIG_IDENTITY_VERSION = 1;

function sortJsonValue(value) {
  if (Array.isArray(value)) return value.map(sortJsonValue);
  if (!value || typeof value !== 'object') return value;
  const sorted = {};
  for (const key of Object.keys(value).sort()) {
    sorted[key] = sortJsonValue(value[key]);
  }
  return sorted;
}

// Config identity is defined over JSON data after schema normalization. The
// JSON round trip deliberately applies the same unsupported-value behavior as
// transport serialization before object keys are sorted recursively.
export function canonicalConfig(value) {
  const serialized = JSON.stringify(value ?? null);
  if (serialized === undefined) {
    throw new TypeError('Configuration identity requires a JSON-serializable value');
  }
  return sortJsonValue(JSON.parse(serialized));
}

export function canonicalConfigJson(value) {
  return JSON.stringify(canonicalConfig(value));
}

export function equivalentConfig(left, right) {
  return canonicalConfigJson(left) === canonicalConfigJson(right);
}

export async function configDigest(value) {
  const subtle = globalThis.crypto?.subtle;
  if (!subtle) throw new Error('SHA-256 is unavailable in this runtime');
  const bytes = new TextEncoder().encode(canonicalConfigJson(value));
  const hash = await subtle.digest('SHA-256', bytes);
  return Array.from(new Uint8Array(hash), (byte) => byte.toString(16).padStart(2, '0')).join('');
}
