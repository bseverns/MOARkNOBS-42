function formatBrightnessCap(value) {
  const numeric = Number(value);
  return Number.isFinite(numeric) ? `${numeric}/255` : '-';
}

function formatRailState(value) {
  if (value === true) return 'verified';
  if (value === false) return 'unverified';
  return 'unknown';
}

export function renderPowerSummary(container, manifest = {}) {
  if (!container) return;
  const railState = formatRailState(manifest?.rail_topology_verified);
  container.innerHTML = '';

  const power = document.createElement('span');
  power.textContent = `Power: ${manifest?.power_profile || '-'}`;

  const cap = document.createElement('span');
  cap.textContent = `LED cap: ${formatBrightnessCap(manifest?.led_brightness_cap)}`;

  const rail = document.createElement('span');
  rail.dataset.railState = railState;
  rail.textContent = `Rail: ${railState.toUpperCase()}`;

  container.append(power, cap, rail);
}

export function createPowerSafetySummary({ containers = [] } = {}) {
  function render(manifest = {}) {
    containers.forEach((container) => renderPowerSummary(container, manifest));
  }

  return { render };
}
