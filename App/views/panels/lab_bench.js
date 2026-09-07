function makeBenchPanel(document, bench, name, label) {
  const panel = document.createElement('section');
  panel.className = `lab-bench-panel utility-panel lab-bench-panel--${name}`;
  panel.dataset.utilityPanel = name;
  panel.setAttribute('aria-label', label);
  bench.append(panel);
  return panel;
}

function makeInstrumentZone(document, name, title, authority) {
  const zone = document.createElement('section');
  zone.className = `instrument-authority-zone instrument-authority-zone--${name}`;
  zone.dataset.instrumentZone = name;
  zone.setAttribute('aria-labelledby', `instrument-${name}-title`);
  zone.innerHTML = `
    <header class="instrument-authority-header">
      <div>
        <p class="workspace-kicker">${authority}</p>
        <h3 id="instrument-${name}-title">${title}</h3>
      </div>
    </header>
  `;
  const content = document.createElement('div');
  content.className = 'instrument-authority-content';
  zone.append(content);
  return { zone, content };
}

function move(document, selector, destination) {
  const element = document.querySelector(selector);
  if (element) destination.append(element);
}

// The Bench owns spatial composition only. Existing panels/controllers retain
// their DOM identities and therefore their event bindings and state ownership.
export function createLabBench({ document, main = document?.querySelector('main') } = {}) {
  if (!document || !main) return null;
  const bench = document.createElement('section');
  bench.id = 'lab-bench';
  bench.dataset.uiTier = 'advanced';
  bench.setAttribute('aria-labelledby', 'lab-bench-title');
  bench.innerHTML = `
    <header class="lab-bench-header">
      <div>
        <p class="workspace-kicker">Lab Bench</p>
        <h2 id="lab-bench-title">Choose a layer of the machine</h2>
        <p class="microcopy">Instrument, profile, observation, and evidence each keep their own authority and purpose.</p>
      </div>
      <div class="lab-bench-tabbar" role="tablist" aria-label="Lab Bench workspaces">
        <button type="button" class="utility-tab" data-utility-tab="instrument" aria-label="Instrument" aria-pressed="true">
          <strong>Instrument</strong>
          <small>Assignments · Filter · ARG · LEDs · USB MIDI · Clock</small>
        </button>
        <button type="button" class="utility-tab" data-utility-tab="profile" aria-label="Profile" aria-pressed="false">
          <strong>Profile</strong>
          <small>Arp · LFO generators &amp; routes · Incoming MIDI</small>
        </button>
        <button type="button" class="utility-tab" data-utility-tab="observe" aria-label="Observe" aria-pressed="false">
          <strong>Observe</strong>
          <small>EF/LFO Scope · MIDI Monitor · Device Monitor · Modulation Matrix</small>
        </button>
        <button type="button" class="utility-tab" data-utility-tab="evidence" aria-label="Evidence" aria-pressed="false">
          <strong>Evidence</strong>
          <small>Staged Diff · Slot Inspector · Debug Log</small>
        </button>
      </div>
    </header>
  `;

  const instrument = makeBenchPanel(document, bench, 'instrument', 'Instrument controls');
  const profile = makeBenchPanel(document, bench, 'profile', 'Profile controls');
  const observe = makeBenchPanel(document, bench, 'observe', 'Observation tools');
  const evidence = makeBenchPanel(document, bench, 'evidence', 'Evidence and diagnostics');
  const configuration = makeInstrumentZone(
    document,
    'configuration',
    'Machine configuration',
    'Apply required'
  );
  const live = makeInstrumentZone(document, 'live', 'Live device controls', 'Writes now');
  instrument.append(configuration.zone, live.zone);

  const consolePanel = document.querySelector('[data-utility-panel="console"]');
  consolePanel?.classList.remove('utility-panel', 'utility-panel-active');
  consolePanel?.removeAttribute('data-utility-panel');
  document.querySelector('.utility-tabbar')?.remove();

  move(document, '.ef-modulation-cluster', configuration.content);
  move(document, '#led-settings', configuration.content);
  const usbMidiCard = document.querySelector('#usb-midi-toggle')?.closest('.live-toggle-card');
  if (usbMidiCard) live.content.append(usbMidiCard);
  const deviceClockCard = document.querySelector('#device-clock-source')?.closest('.live-toggle-card');
  if (deviceClockCard) live.content.append(deviceClockCard);

  move(document, '#profile-performance-workspace', profile);
  move(document, '#device-monitor-section', observe);
  move(document, '#scope-panel', observe);
  move(document, '#midi-panel', observe);
  move(document, '.mod-matrix-card', observe);
  move(document, '#diff-panel', evidence);
  move(document, '#diff-empty', evidence);
  move(document, '#slot-detail-panel', evidence);
  move(document, '.debug-log-bridge', evidence);
  document.querySelector('[data-utility-panel="diff"]')?.remove();
  document.querySelector('[data-utility-panel="midi"]')?.remove();
  document.querySelector('[data-utility-panel="scope"]')?.remove();
  main.append(bench);
  return bench;
}
