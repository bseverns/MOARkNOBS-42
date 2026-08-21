const APPLY_ALLOWED_QUALITIES = new Set(['verified', 'simulator']);

function versionsDiffer(deviceVersion, appVersion) {
  return (
    deviceVersion !== null &&
    deviceVersion !== undefined &&
    appVersion !== null &&
    appVersion !== undefined &&
    String(deviceVersion) !== String(appVersion)
  );
}

export function initialContractQuality({ useSimulator }) {
  return useSimulator ? 'simulator' : 'incompatible';
}

export function resolveStructuredContractQuality({
  useSimulator,
  remoteManifest,
  localManifest,
  schemaSource
}) {
  if (useSimulator) return 'simulator';
  if (versionsDiffer(remoteManifest?.schema_version, localManifest?.schema_version)) {
    return 'migration-required';
  }
  return schemaSource === 'device' ? 'verified' : 'fallback-schema';
}

export function resolveHydratedContractQuality({
  useSimulator,
  remoteManifest,
  localManifest,
  handshakeQuality,
  schemaQuality
}) {
  if (useSimulator) return 'simulator';
  if (versionsDiffer(remoteManifest?.schema_version, localManifest?.schema_version)) {
    return 'migration-required';
  }
  return handshakeQuality === 'verified' ? schemaQuality : handshakeQuality;
}

export function contractQualityStatus(quality) {
  return { quality, applyAllowed: APPLY_ALLOWED_QUALITIES.has(quality) };
}

export function assertApplyAllowed(quality) {
  if (!APPLY_ALLOWED_QUALITIES.has(quality)) {
    throw new Error(
      `Apply is blocked until the device contract is verified (current: ${quality}).`
    );
  }
}

export function selectConfigReadRpc({ useSimulator, remoteManifest }) {
  return !useSimulator && remoteManifest?.capabilities?.chunked_reads?.config
    ? 'get_config_chunked'
    : 'get_config';
}

