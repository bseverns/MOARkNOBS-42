function buildFallbackManifest(localManifest, argMethodCount) {
  return {
    device_name: localManifest?.device_name ?? 'MOARkNOBS-42',
    fw_version: 'unknown',
    git_sha: 'offline',
    build_time: new Date().toISOString(),
    schema_version: localManifest?.schema_version,
    slot_count: localManifest?.slot_count,
    pot_count: localManifest?.pot_count,
    envelope_count: localManifest?.envelope_count,
    arg_method_count: argMethodCount,
    led_count: localManifest?.led_count ?? 0,
    free_ram: 0,
    free_flash: 0
  };
}

export async function performConnectionHandshake({
  sendRpc,
  emit,
  localManifest,
  localSlotMetaManager,
  migrations = {},
  argMethodCount
} = {}) {
  try {
    await sendRpc({ rpc: 'hello' });
  } catch (err) {
    console.debug('hello RPC failed', err);
  }

  let manifestPayload;
  try {
    manifestPayload = await sendRpc({ rpc: 'get_manifest' });
  } catch (err) {
    console.debug('get_manifest RPC failed', err);
    manifestPayload = null;
  }

  const manifestData = manifestPayload?.manifest ?? manifestPayload;
  const remoteManifest =
    manifestData && typeof manifestData === 'object'
      ? manifestData
      : buildFallbackManifest(localManifest, argMethodCount);

  localSlotMetaManager.ensureCount(remoteManifest?.slot_count ?? localManifest?.slot_count ?? 0);
  emit('manifest', remoteManifest);

  if (!remoteManifest.schema_version && remoteManifest.schemaVersion) {
    remoteManifest.schema_version = remoteManifest.schemaVersion;
  }
  if (localManifest && remoteManifest.schema_version !== localManifest.schema_version) {
    const key = `${remoteManifest.schema_version}->${localManifest.schema_version}`;
    emit('migration-required', {
      from: remoteManifest.schema_version,
      to: localManifest.schema_version,
      canAdapt: typeof migrations[key] === 'function'
    });
  }

  return remoteManifest;
}
