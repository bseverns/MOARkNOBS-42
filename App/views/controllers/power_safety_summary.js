function formatBrightnessCap(value) {
  const numeric = Number(value);
  return Number.isFinite(numeric) ? `${numeric}/255` : 'unavailable';
}

function formatRailState(value) {
  if (value === true) return 'verified';
  if (value === false) return 'unverified';
  return 'unknown';
}

export function renderPowerSummary(
  container,
  manifest = {},
  { connected = false, warningOnly = false, includeWarning = true } = {}
) {
  if (!container) return;
  container.innerHTML = '';
  container.dataset.powerWarning = 'false';
  container.dataset.powerAvailable = connected ? 'true' : 'false';
  if (warningOnly) container.setAttribute('hidden', '');

  if (!connected) {
    if (warningOnly) return;
    const unavailable = document.createElement('span');
    unavailable.className = 'power-status-unavailable';
    unavailable.textContent = 'Power status unavailable';
    container.append(unavailable);
    return;
  }

  const profile =
    typeof manifest?.power_profile === 'string' && manifest.power_profile.trim()
      ? manifest.power_profile.trim()
      : 'unavailable';
  const railState = formatRailState(manifest?.rail_topology_verified);

  if (warningOnly) {
    if (profile !== 'POWER_CHOKED_V1') return;
    container.dataset.powerWarning = 'true';
    container.removeAttribute('hidden');
    container.textContent =
      'Power-limited hardware reported. Keep LED brightness within the device-reported cap and avoid full-brightness LED tests.';
    return;
  }

  const power = document.createElement('span');
  power.textContent = `Power: ${profile}`;

  const cap = document.createElement('span');
  cap.textContent = `LED cap: ${formatBrightnessCap(manifest?.led_brightness_cap)}`;

  const rail = document.createElement('span');
  rail.dataset.railState = railState;
  rail.textContent = `Rail: ${railState.toUpperCase()}`;

  container.append(power, cap, rail);

  if (profile !== 'POWER_CHOKED_V1' || !includeWarning) return;

  container.dataset.powerWarning = 'true';
  const warning = document.createElement('div');
  warning.className = 'power-safety-warning';
  warning.dataset.powerWarning = 'true';
  warning.textContent =
    'Power-limited hardware reported. Keep LED brightness within the device-reported cap and avoid full-brightness LED tests.';
  container.append(warning);
}

export function createPowerSafetySummary({ containers = [], warningContainers = [] } = {}) {
  function render(manifest = {}, options = {}) {
    containers.forEach((container) =>
      renderPowerSummary(container, manifest, { ...options, includeWarning: false })
    );
    warningContainers.forEach((container) =>
      renderPowerSummary(container, manifest, { ...options, warningOnly: true })
    );
  }

  return { render };
}
