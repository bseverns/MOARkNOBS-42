function normalizeString(value, fallback = 'Unknown') {
  if (typeof value !== 'string') return fallback;
  const trimmed = value.trim();
  return trimmed || fallback;
}

function inferBrowserName(nav = {}) {
  const brands = Array.isArray(nav?.userAgentData?.brands) ? nav.userAgentData.brands : [];
  const brandMatch = brands.find((entry) =>
    /Chrom|Edge|Opera|Brave|Vivaldi/i.test(entry?.brand ?? '')
  );
  if (brandMatch?.brand) return normalizeString(brandMatch.brand);

  const ua = normalizeString(nav?.userAgent, '');
  if (/Edg\//i.test(ua)) return 'Microsoft Edge';
  if (/OPR\//i.test(ua)) return 'Opera';
  if (/Brave\//i.test(ua)) return 'Brave';
  if (/Vivaldi\//i.test(ua)) return 'Vivaldi';
  if (/Chrome\//i.test(ua) || /Chromium\//i.test(ua)) return 'Google Chrome';
  if (/Safari\//i.test(ua) && !/Chrome\//i.test(ua)) return 'Safari';
  if (/Firefox\//i.test(ua)) return 'Firefox';
  return 'Unknown browser';
}

function inferPlatform(nav = {}) {
  return normalizeString(nav?.userAgentData?.platform ?? nav?.platform, 'Unknown OS');
}

function isLikelyChromiumBrowser(browserName) {
  return /Chrom|Edge|Opera|Brave|Vivaldi/i.test(browserName);
}

function buildSecureContextHint(location = {}) {
  const origin = normalizeString(location?.origin, '');
  if (origin && origin !== 'null') {
    return `Reload this app from ${origin} only if it is served over https:// or http://localhost.`;
  }
  return 'Reload this app from https:// or http://localhost so Web Serial is allowed.';
}

export function getWebSerialCompatibility({
  nav = globalThis.navigator,
  win = globalThis.window,
  location = globalThis.location
} = {}) {
  const browser = inferBrowserName(nav);
  const platform = inferPlatform(nav);
  const secureContext = Boolean(win?.isSecureContext ?? globalThis.isSecureContext ?? false);
  const hasSerialObject = Boolean(nav?.serial);
  const canRequestPort = typeof nav?.serial?.requestPort === 'function';
  const likelyChromium = isLikelyChromiumBrowser(browser);

  const issues = [];
  if (!likelyChromium && !canRequestPort) {
    issues.push('Use Chrome or Edge on desktop for the direct USB configurator.');
  }
  if (!secureContext) {
    issues.push(buildSecureContextHint(location));
  }
  if (!hasSerialObject || !canRequestPort) {
    issues.push('This page cannot access the Web Serial API in the current browser/session.');
  }

  const compatible = secureContext && canRequestPort;
  const state = compatible ? 'ok' : 'err';
  const label = compatible ? 'Compatibility OK' : 'Web Serial unavailable';
  const message = compatible
    ? `${browser} on ${platform} can request a serial device from this page.`
    : issues[0] ?? 'This browser session cannot access Web Serial. Use Chrome or Edge on desktop.';

  return {
    compatible,
    state,
    label,
    message,
    browser,
    platform,
    secureContext,
    hasSerialObject,
    canRequestPort,
    likelyChromium,
    issues
  };
}

export function explainTransportError(
  error,
  { action = 'connect', compatibility = null, usingBridge = false, usingSimulator = false } = {}
) {
  const message = normalizeString(error?.message, String(error ?? 'Unknown error'));
  const name = normalizeString(error?.name, '');
  const support =
    compatibility ??
    (usingBridge || usingSimulator
      ? null
      : getWebSerialCompatibility({
          nav: globalThis.navigator,
          win: globalThis.window,
          location: globalThis.location
        }));

  if (usingSimulator) {
    return {
      label: action === 'config-boot' ? 'Config boot unavailable' : 'Simulator error',
      message:
        action === 'config-boot'
          ? 'Configurator boot only works against a physical device, not the simulator.'
          : message
    };
  }

  if (usingBridge) {
    if (/WebSocket/i.test(message)) {
      return {
        label: 'Bridge connection failed',
        message: 'The WebSocket bridge is unavailable. Start the bridge and try reconnecting.'
      };
    }
    return {
      label: action === 'config-boot' ? 'Bridge request failed' : 'Bridge error',
      message
    };
  }

  if (name === 'NotFoundError') {
    return {
      label: 'No device selected',
      message: 'Choose the MOARkNOBS-42 serial device from the browser picker to continue.'
    };
  }

  if (name === 'SecurityError') {
    return {
      label: 'Serial permission blocked',
      message:
        support && !support.secureContext
          ? buildSecureContextHint(globalThis.location)
          : 'The browser blocked serial access. Recheck OS permissions and try Chrome or Edge.'
    };
  }

  if (/WebSerial unavailable/i.test(message)) {
    return {
      label: support?.label ?? 'Web Serial unavailable',
      message:
        support?.message ??
        'Use Chrome or Edge on desktop and serve the app from https:// or http://localhost.'
    };
  }

  if (/RPC timeout/i.test(message)) {
    return {
      label: action === 'config-boot' ? 'Config boot timeout' : 'Connection timed out',
      message:
        action === 'config-boot'
          ? 'The device did not acknowledge the config-boot request. Make sure nothing else has the serial port open.'
          : 'Handshake timed out. Confirm the device is connected, close other serial apps, and try again.'
    };
  }

  if (
    name === 'NetworkError' ||
    name === 'InvalidStateError' ||
    /Writer unavailable|Native port closed|port close|already open/i.test(message)
  ) {
    return {
      label: 'Serial port unavailable',
      message:
        'The serial port is busy or disappeared. Close other serial apps, replug USB, and try again.'
    };
  }

  if (/WebSocket/i.test(message)) {
    return {
      label: 'Bridge connection failed',
      message: 'The WebSocket bridge disconnected before the configurator finished connecting.'
    };
  }

  return {
    label: action === 'config-boot' ? 'Config boot failed' : 'Connection failed',
    message:
      action === 'config-boot'
        ? 'Could not request configurator boot. Is the device connected and idle on USB?'
        : 'Could not complete the device handshake. Is the device connected and in config mode?'
  };
}
