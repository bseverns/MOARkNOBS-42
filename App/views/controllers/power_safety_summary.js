import {
  MN42_LED_BRIGHTNESS_CAP,
  MN42_POWER_PROFILE,
  MN42_RAIL_TOPOLOGY_VERIFIED
} from '../../manifest_contract.js';

function formatBrightnessCap(value) {
  const numeric = Number(value);
  return Number.isFinite(numeric) ? `${numeric}/255` : '-';
}

function formatRailState(value) {
  if (value === true) return 'verified';
  if (value === false) return 'unverified';
  return 'unknown';
}

function buildReleaseBoundaryMismatch(manifest = {}) {
  const reportedProfile =
    typeof manifest?.power_profile === 'string' ? manifest.power_profile : null;
  const reportedRailVerified = manifest?.rail_topology_verified;
  const reportedCap = Number(manifest?.led_brightness_cap);
  const profileMismatch = reportedProfile === 'SPLIT_RAIL_REWORK';
  const railMismatch = reportedRailVerified === true;

  if (!profileMismatch && !railMismatch) {
    return null;
  }

  return {
    reportedProfile: reportedProfile || '-',
    reportedCap: Number.isFinite(reportedCap) ? `${reportedCap}/255` : '-',
    reportedRail: formatRailState(reportedRailVerified).toUpperCase(),
    expectedProfile: MN42_POWER_PROFILE,
    expectedCap: `${MN42_LED_BRIGHTNESS_CAP}/255`,
    expectedRail: formatRailState(MN42_RAIL_TOPOLOGY_VERIFIED).toUpperCase()
  };
}

export function renderPowerSummary(container, manifest = {}) {
  if (!container) return;
  const railState = formatRailState(manifest?.rail_topology_verified);
  const mismatch = buildReleaseBoundaryMismatch(manifest);
  container.innerHTML = '';
  container.dataset.powerMismatch = mismatch ? 'true' : 'false';

  const power = document.createElement('span');
  power.textContent = `Power: ${manifest?.power_profile || '-'}`;

  const cap = document.createElement('span');
  cap.textContent = `LED cap: ${formatBrightnessCap(manifest?.led_brightness_cap)}`;

  const rail = document.createElement('span');
  rail.dataset.railState = railState;
  rail.textContent = `Rail: ${railState.toUpperCase()}`;

  container.append(power, cap, rail);

  if (!mismatch) {
    return;
  }

  const warning = document.createElement('div');
  warning.className = 'power-safety-warning';
  warning.dataset.powerMismatch = 'true';
  warning.textContent =
    `Release boundary mismatch: firmware reports ${mismatch.reportedProfile}, LED cap ${mismatch.reportedCap}, ` +
    `rail ${mismatch.reportedRail}. Current repo-safe expectation is ${mismatch.expectedProfile}, ` +
    `LED cap ${mismatch.expectedCap}, rail ${mismatch.expectedRail} for Rev A or otherwise undocumented ` +
    `topology. Avoid full-brightness LED tests until rail topology is documented.`;
  container.append(warning);
}

export function createPowerSafetySummary({ containers = [] } = {}) {
  function render(manifest = {}) {
    containers.forEach((container) => renderPowerSummary(container, manifest));
  }

  return { render };
}
