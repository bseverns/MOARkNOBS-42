export function deriveWebSocketUrl(baseUrl, path) {
  if (!baseUrl || !path) return null;
  try {
    const base = new URL(baseUrl, typeof window !== 'undefined' ? window.location.href : undefined);
    const protocol = base.protocol === 'https:' ? 'wss:' : 'ws:';
    return new URL(path, `${protocol}//${base.host}`).toString();
  } catch (_) {
    return null;
  }
}

export function resolveTransportModeOptions({
  locationHref,
  bridgeApiBaseUrl,
  bridgeTransportMode,
  wsUrl
} = {}) {
  const currentLocation =
    typeof window !== 'undefined' && typeof window.location === 'object'
      ? new URL(window.location.href)
      : null;
  const params = locationHref ? new URL(locationHref).searchParams : null;
  const inferredBridgeBaseUrl =
    currentLocation && currentLocation.pathname.startsWith('/app/') ? currentLocation.origin : null;
  const structuredBridgePreference =
    bridgeTransportMode ??
    params?.get('bridgeTransport')?.trim() ??
    (inferredBridgeBaseUrl ? 'session' : null);
  const resolvedBridgeApiBaseUrl =
    typeof bridgeApiBaseUrl === 'string' && bridgeApiBaseUrl.trim().length
      ? new URL(bridgeApiBaseUrl, locationHref).toString()
      : inferredBridgeBaseUrl;
  const websocketUrl =
    typeof wsUrl === 'string' && wsUrl.trim().length
      ? wsUrl.trim()
      : params?.get('ws')?.trim() ?? deriveWebSocketUrl(resolvedBridgeApiBaseUrl, '/ws') ?? null;

  return {
    structuredBridgePreference,
    resolvedBridgeApiBaseUrl,
    websocketUrl,
    bridgeEventsUrl: deriveWebSocketUrl(resolvedBridgeApiBaseUrl, '/ws/events')
  };
}

export function wantsStructuredBridgeSession({
  useSimulator = false,
  structuredBridgePreference,
  resolvedBridgeApiBaseUrl
} = {}) {
  return !useSimulator && structuredBridgePreference === 'session' && !!resolvedBridgeApiBaseUrl;
}

export function getTransportMode({
  useSimulator = false,
  bridgeSessionActive = false,
  websocketUrl
} = {}) {
  if (useSimulator) return 'simulator';
  if (bridgeSessionActive) return 'bridge-session';
  if (websocketUrl) return 'bridge-raw';
  return 'direct-webserial';
}
