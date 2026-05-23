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

function formatMicros(value) {
  const numeric = Number(value);
  return Number.isFinite(numeric) ? `${Math.round(numeric)} us` : '-';
}

function formatBpm(value) {
  const numeric = Number(value);
  return Number.isFinite(numeric) && numeric > 0 ? numeric.toFixed(1) : '-';
}

export function createDeviceMonitorController({ container, resolveDeviceName } = {}) {
  let lastManifest = {};
  let lastTelemetry = {};

  function render() {
    if (!container) return;
    container.innerHTML = '';

    const diagnostics =
      lastTelemetry?.diagnostics && typeof lastTelemetry.diagnostics === 'object'
        ? lastTelemetry.diagnostics
        : {};
    const clock =
      lastTelemetry?.clock && typeof lastTelemetry.clock === 'object' ? lastTelemetry.clock : {};

    const entries = {
      Device: resolveDeviceName?.(lastManifest) ?? lastManifest?.device_name ?? '-',
      Firmware: lastManifest?.fw_version || '-',
      'Git SHA': lastManifest?.git_sha ? lastManifest.git_sha.slice(0, 8) : '-',
      'Build time': lastManifest?.build_time || '-',
      'Schema version': lastManifest?.schema_version ?? '-',
      'Power profile': lastManifest?.power_profile || '-',
      'LED cap': formatBrightnessCap(lastManifest?.led_brightness_cap),
      'Rail verified': formatBoolean(lastManifest?.rail_topology_verified),
      'Free RAM': formatKiB(lastManifest?.free_ram),
      'Free Flash': formatKiB(lastManifest?.free_flash),
      'Clock source': typeof clock?.source === 'string' ? clock.source : '-',
      'Follow ext': formatBoolean(clock?.follow_external),
      'Clock out': formatBoolean(clock?.clock_out_enabled),
      'Clock running': formatBoolean(clock?.running),
      'External signal': formatBoolean(clock?.external_signal),
      'Tapped BPM': formatBpm(clock?.tapped_bpm),
      'External BPM': formatBpm(clock?.external_bpm),
      'Loop max': formatMicros(diagnostics?.loop_max_us),
      'Loop last': formatMicros(diagnostics?.loop_last_us),
      'MIDI ISR max': formatMicros(diagnostics?.midi_isr_max_us),
      'MIDI ISR last': formatMicros(diagnostics?.midi_isr_last_us),
      'Loop overruns': diagnostics?.loop_overruns ?? '-',
      'MIDI task overruns': diagnostics?.midi_task_overruns ?? '-',
      'MIDI drops': diagnostics?.midi_drops ?? '-',
      'UART overruns': diagnostics?.uart_overruns ?? '-'
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

  function renderManifest(manifest = {}) {
    lastManifest = manifest || {};
    render();
  }

  function renderTelemetry(frame = {}) {
    lastTelemetry = frame || {};
    render();
  }

  return { renderManifest, renderTelemetry };
}
