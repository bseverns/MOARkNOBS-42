import { createExplainerIcon, explainerIconMarkup } from '../explainer_icons.js';

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
  const icon = name === 'configuration' ? 'controls' : 'live';
  zone.innerHTML = `
    <header class="instrument-authority-header explainer-heading">
      ${explainerIconMarkup(icon)}
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

function decorateHeading(document, selector, icon) {
  const heading = document.querySelector(selector);
  if (!heading || heading.querySelector(':scope > .explainer-icon')) return;
  heading.classList.add('explainer-heading');
  heading.prepend(createExplainerIcon(document, icon));
}

function decorateSummary(document, selector, icon) {
  const summary = document.querySelector(selector);
  if (!summary || summary.querySelector(':scope > .explainer-icon')) return;
  summary.classList.add('explainer-summary');
  summary.prepend(createExplainerIcon(document, icon));
}

function decorateTab(document, selector, icon) {
  const tab = document.querySelector(selector);
  if (!tab || tab.querySelector(':scope > .explainer-icon')) return;
  tab.classList.add('explainer-tab');
  tab.prepend(createExplainerIcon(document, icon, 'explainer-icon--tab'));
}

function decorateLegend(document, selector, icon) {
  const legend = document.querySelector(`${selector} > legend`);
  if (!legend || legend.querySelector(':scope > .explainer-icon')) return;
  legend.classList.add('explainer-legend');
  legend.prepend(createExplainerIcon(document, icon, 'explainer-icon--legend'));
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
          ${explainerIconMarkup('instrument', 'explainer-icon--tab')}
          <span><strong>Instrument</strong><small>Assignments · Filter · ARG · LEDs · USB MIDI · Clock</small></span>
        </button>
        <button type="button" class="utility-tab" data-utility-tab="profile" aria-label="Profile" aria-pressed="false">
          ${explainerIconMarkup('profile', 'explainer-icon--tab')}
          <span><strong>Profile</strong><small>Arp · LFO generators &amp; routes · Incoming MIDI</small></span>
        </button>
        <button type="button" class="utility-tab" data-utility-tab="observe" aria-label="Observe" aria-pressed="false">
          ${explainerIconMarkup('observe', 'explainer-icon--tab')}
          <span><strong>Observe</strong><small>EF/LFO Scope · MIDI Monitor · Device Monitor · Modulation Matrix</small></span>
        </button>
        <button type="button" class="utility-tab" data-utility-tab="evidence" aria-label="Evidence" aria-pressed="false">
          ${explainerIconMarkup('evidence', 'explainer-icon--tab')}
          <span><strong>Evidence</strong><small>Staged Diff · Slot Inspector · Debug Log</small></span>
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
  main.append(bench);
  decorateHeading(document, '#profile-performance-workspace .profile-performance-header > div', 'profile');
  decorateTab(document, '[data-performance-tab="arp"]', 'arp');
  decorateTab(document, '[data-performance-tab="lfo"]', 'routes');
  decorateTab(document, '[data-performance-tab="incoming"]', 'incoming');
  decorateHeading(document, '#arp-profile-card .macro-card-header', 'arp');
  decorateHeading(document, '#lfo-profile-card .macro-card-header', 'routes');
  decorateHeading(document, '#midi-input-form > header', 'incoming');
  decorateHeading(document, '#device-monitor-section .monitor-header > div', 'device');
  decorateHeading(document, '#scope-panel .scope-panel-header > div', 'scope');
  decorateHeading(document, '#midi-panel .midi-panel-header > div', 'midi');
  decorateHeading(document, '.mod-matrix-card .live-toggle-card-header > div', 'matrix');
  decorateHeading(document, '#diff-empty', 'diff');
  decorateHeading(document, '#diff-panel', 'diff');
  decorateHeading(document, '#slot-detail-panel .slot-detail-panel-header > div', 'inspector');
  decorateLegend(document, '#ef-assignment-card', 'assignments');
  decorateLegend(document, '#filter-settings', 'filter');
  decorateLegend(document, '#arg-settings', 'arg');
  decorateLegend(document, '#envelope-mode-settings', 'display');
  decorateLegend(document, '#led-settings', 'led');
  decorateHeading(document, '#usb-midi-toggle-title', 'usb');
  decorateHeading(document, '#device-clock-title', 'clock');
  decorateSummary(document, '.debug-log-bridge > summary', 'debug');
  document.querySelector('[data-utility-panel="diff"]')?.remove();
  document.querySelector('[data-utility-panel="midi"]')?.remove();
  document.querySelector('[data-utility-panel="scope"]')?.remove();
  return bench;
}
