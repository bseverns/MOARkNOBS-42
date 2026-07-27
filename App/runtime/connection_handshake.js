function buildFallbackManifest(localManifest, argMethodCount) {
  return {
    device_name: localManifest?.device_name ?? 'MOARkNOBS-42',
    fw_version: 'unknown',
    git_sha: 'offline',
    // This is intentionally unknown. Browser time is not firmware build time.
    build_time: null,
    schema_version: localManifest?.schema_version,
    slot_count: localManifest?.slot_count,
    pot_count: localManifest?.pot_count,
    envelope_count: localManifest?.envelope_count,
    arg_method_count: argMethodCount,
    led_count: localManifest?.led_count ?? 0,
    power_profile: localManifest?.power_profile,
    led_brightness_cap: localManifest?.led_brightness_cap,
    rail_topology_verified: localManifest?.rail_topology_verified,
    capabilities:
      localManifest?.capabilities && typeof localManifest.capabilities === 'object'
        ? { ...localManifest.capabilities }
        : {},
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
  let helloVerified = true;
  try {
    await sendRpc({ rpc: 'hello' });
  } catch (err) {
    helloVerified = false;
    emit('status', {
      stage: 'handshake',
      level: 'warn',
      message: `HELLO verification failed: ${err.message || String(err)}`
    });
  }

  let manifestPayload;
  try {
    manifestPayload = await sendRpc({ rpc: 'get_manifest' });
  } catch (err) {
    emit('status', {
      stage: 'handshake',
      level: 'warn',
      message: `Manifest retrieval failed; using fail-closed fallback: ${err.message || String(err)}`
    });
    manifestPayload = null;
  }

  const manifestData = manifestPayload?.manifest ?? manifestPayload;
  const manifestVerified = Boolean(manifestData && typeof manifestData === 'object');
  const remoteManifest = manifestVerified
    ? manifestData
    : buildFallbackManifest(localManifest, argMethodCount);

  const quality = !manifestVerified
    ? 'fallback-manifest'
    : helloVerified
      ? 'verified'
      : 'incompatible';
  remoteManifest.contract_quality = quality;
  remoteManifest.manifest_source = manifestVerified ? 'device' : 'fallback';

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

  return { manifest: remoteManifest, quality, helloVerified };
}
