// Ensure device-supplied schemas include the structures this runtime expects.
export function isRuntimeCompatibleSchema(candidate) {
  if (!candidate || typeof candidate !== 'object' || Array.isArray(candidate)) return false;
  if (candidate.type && candidate.type !== 'object') return false;
  if (!candidate.properties || typeof candidate.properties !== 'object') return false;
  const requiredRoots = ['slots', 'efSlots', 'filter', 'arg', 'led'];
  return requiredRoots.every((key) => {
    const branch = candidate.properties[key];
    return branch && typeof branch === 'object';
  });
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
    console.debug('get_schema RPC failed', err);
  }
  if (isRuntimeCompatibleSchema(deviceSchema)) {
    return { schema: deviceSchema, source: 'device' };
  }

  const bundledSchema = await fetchJson(schemaUrl);
  if (!isRuntimeCompatibleSchema(bundledSchema)) {
    throw new Error('Bundled schema is incompatible with runtime requirements');
  }
  if (deviceSchema) {
    emit?.('status', {
      stage: 'schema',
      level: 'warn',
      message: 'Device schema is incompatible; using bundled schema.'
    });
  }
  return { schema: bundledSchema, source: 'bundled' };
}
