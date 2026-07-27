// Ensure device-supplied schemas include the structures this runtime expects.
export function isRuntimeCompatibleSchema(candidate) {
  if (!candidate || typeof candidate !== 'object' || Array.isArray(candidate)) return false;
  if (candidate.type && candidate.type !== 'object') return false;
  if (!candidate.properties || typeof candidate.properties !== 'object') return false;
  const requiredRoots = ['slots', 'efSlots', 'filter', 'arg', 'led'];
  if (!Array.isArray(candidate.required) || !requiredRoots.every((key) => candidate.required.includes(key))) {
    return false;
  }
  return requiredRoots.every((key) => {
    const branch = candidate.properties[key];
    return branch && typeof branch === 'object';
  });
}

const MINI_AJV_KEYWORDS = new Set([
  '$schema', '$id', 'title', 'description', 'default', 'examples', 'deprecated', 'readOnly', 'writeOnly',
  'type', 'enum', 'minimum', 'maximum', 'maxLength', 'pattern', 'required', 'additionalProperties',
  'properties', 'anyOf', 'minItems', 'maxItems', 'items', 'uniqueItems'
]);

// Reject constraints MiniAjv would otherwise silently ignore. Property names and enum values are data,
// not schema keywords, so only recurse into values that are themselves schemas.
export function findUnsupportedSchemaKeywords(schema, path = '#') {
  if (!schema || typeof schema !== 'object' || Array.isArray(schema)) return [];
  const unsupported = [];
  for (const [key, value] of Object.entries(schema)) {
    if (!MINI_AJV_KEYWORDS.has(key)) unsupported.push(`${path}/${key}`);
    if (key === 'properties' && value && typeof value === 'object') {
      for (const [property, child] of Object.entries(value)) {
        unsupported.push(...findUnsupportedSchemaKeywords(child, `${path}/properties/${property}`));
      }
    } else if (key === 'items') {
      unsupported.push(...findUnsupportedSchemaKeywords(value, `${path}/items`));
    } else if (key === 'anyOf' && Array.isArray(value)) {
      value.forEach((child, index) => unsupported.push(...findUnsupportedSchemaKeywords(child, `${path}/anyOf/${index}`)));
    }
  }
  return unsupported;
}

export async function selectSchemaForHydration({
  sendRpc,
  schemaUrl,
  emit,
  fetchJson = async (url) => {
    const response = await fetch(url);
    return response.json();
  }
} = {}) {
  let deviceSchema = null;
  try {
    const response = await sendRpc({ rpc: 'get_schema' });
    deviceSchema = response?.schema ?? response ?? null;
  } catch (err) {
    emit?.('status', {
      stage: 'schema',
      level: 'warn',
      message: `Schema retrieval failed; using bundled fallback: ${err.message || String(err)}`
    });
  }
  const unsupportedKeywords = findUnsupportedSchemaKeywords(deviceSchema);
  if (isRuntimeCompatibleSchema(deviceSchema) && unsupportedKeywords.length === 0) {
    return { schema: deviceSchema, source: 'device', quality: 'verified' };
  }

  const bundledSchema = await fetchJson(schemaUrl);
  if (!isRuntimeCompatibleSchema(bundledSchema)) {
    throw new Error('Bundled schema is incompatible with runtime requirements');
  }
  if (deviceSchema) {
    emit?.('status', {
      stage: 'schema',
      level: 'warn',
      message: unsupportedKeywords.length
        ? `Device schema uses unsupported constraints (${unsupportedKeywords.join(', ')}); using bundled schema.`
        : 'Device schema is incompatible; using bundled schema.'
    });
  }
  return {
    schema: bundledSchema,
    source: 'bundled',
    quality: deviceSchema ? 'incompatible' : 'fallback-schema'
  };
}
