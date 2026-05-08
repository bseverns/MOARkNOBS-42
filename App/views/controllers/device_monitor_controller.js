function formatKiB(value) {
  const numeric = Number(value);
  return Number.isFinite(numeric) ? `${Math.round(numeric / 1024)} KiB` : '-';
}

function formatBoolean(value) {
  if (value === true) return 'yes';
  if (value === false) return 'no';
  return '-';
}

function formatBrightnessCap(value) {
  const numeric = Number(value);
  return Number.isFinite(numeric) ? `${numeric}/255` : '-';
}

export function createDeviceMonitorController({ container, resolveDeviceName } = {}) {
  function render(manifest = {}) {
    if (!container) return;
    container.innerHTML = '';
    const entries = {
      Device: resolveDeviceName?.(manifest) ?? manifest?.device_name ?? '-',
      Firmware: manifest?.fw_version || '-',
      'Git SHA': manifest?.git_sha ? manifest.git_sha.slice(0, 8) : '-',
      'Build time': manifest?.build_time || '-',
      'Schema version': manifest?.schema_version ?? '-',
      'Power profile': manifest?.power_profile || '-',
      'LED cap': formatBrightnessCap(manifest?.led_brightness_cap),
      'Rail verified': formatBoolean(manifest?.rail_topology_verified),
      'Free RAM': formatKiB(manifest?.free_ram),
      'Free Flash': formatKiB(manifest?.free_flash)
    };
    Object.entries(entries).forEach(([label, value]) => {
      const row = document.createElement('div');
      row.className = 'monitor-row';
      const term = document.createElement('span');
      term.className = 'monitor-label';
      term.textContent = label;
      const val = document.createElement('span');
      val.className = 'monitor-value';
      val.textContent = value;
      row.append(term, val);
      container.appendChild(row);
    });
  }

  return { render };
}
