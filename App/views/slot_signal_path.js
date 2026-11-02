const finite = (value) => value !== null && value !== undefined && Number.isFinite(Number(value));
const numberOrNull = (value) => (finite(value) ? Number(value) : null);
const textValue = (value) => (value === null ? '—' : String(value));
const signedValue = (value) =>
  value === null ? 'Not reported' : `${value > 0 ? '+' : ''}${value}`;

// Contribution objects are one ordered resolver snapshot. Never reconstruct
// live arithmetic from staged settings or combine separate telemetry chunks.
export function describeSlotSignal({
  slot = {},
  telemetry = {},
  index = 0,
  connected = false,
  now = Date.now()
} = {}) {
  telemetry = telemetry || {};
  slot = slot || {};
  const age = finite(telemetry.receivedAt) ? now - Number(telemetry.receivedAt) : Infinity;
  const freshness = !connected
    ? 'Disconnected'
    : age > 3000
      ? 'Stale telemetry'
      : age > 1000
        ? 'Delayed telemetry'
        : 'Live telemetry';
  const entry = telemetry.slotContributions?.find((candidate) => candidate?.index === index);
  const evidenceAge = now - Number(telemetry.contributionsReceivedAt ?? telemetry.receivedAt ?? 0);
  const coherent =
    entry &&
    finite(entry.baseline) &&
    finite(entry.output) &&
    finite(entry.ef) &&
    Array.isArray(entry.lfos) &&
    entry.lfos.length === 2 &&
    entry.lfos.every(finite) &&
    Number(entry.baseline) + Number(entry.ef) + Number(entry.lfos[0]) + Number(entry.lfos[1]) ===
      Number(entry.output);
  const measured = connected && age <= 3000 && evidenceAge <= 3000 && coherent;
  const activeMask = measured ? Number(entry.activeMask) || 0 : 0;
  const efIndex = numberOrNull(slot.efIndex ?? slot.ef?.index);
  return {
    freshness,
    baseline: connected ? numberOrNull(measured ? entry.baseline : telemetry.slots?.[index]) : null,
    output: connected
      ? numberOrNull(measured ? entry.output : telemetry.slotOutputs?.[index])
      : null,
    reactive: measured && (activeMask & 0x01) ? Number(entry.ef) : null,
    lfos: measured
      ? entry.lfos.map((value, lfoIndex) =>
          activeMask & (0x02 << lfoIndex) ? Number(value) : null
        )
      : [null, null],
    source:
      efIndex === null || efIndex < 0
        ? 'No EF source assigned'
        : slot.arg?.enabled
          ? `ARG · EF ${Number(slot.arg.sourceA ?? 0) + 1} + EF ${Number(slot.arg.sourceB ?? 1) + 1} → EF shaping`
          : `EF ${efIndex + 1} → EF shaping`,
    measured: Boolean(measured)
  };
}

export function renderSlotSignal(
  container,
  { slot, liveSlot, telemetry, index, connected, dirty, onNavigate = () => {} } = {}
) {
  const signal = describeSlotSignal({ slot: liveSlot, telemetry, index, connected });
  container.replaceChildren();
  container.dataset.freshness = signal.freshness;
  const heading = document.createElement('div');
  heading.className = 'slot-signal-heading';
  const title = document.createElement('strong');
  title.textContent = `${index + 1} · ${slot?.label || 'Selected slot'} · ${slot?.type || 'Off'}${['CC', 'Note', 'ProgramChange', 'NRPN', 'RPN'].includes(slot?.type) && slot?.data1 !== undefined ? ` ${slot.data1}` : ''} · Ch ${slot?.midiChannel ?? '—'}`;
  const output = document.createElement('strong');
  output.className = 'slot-signal-output';
  output.textContent = `Output ${textValue(signal.output)}`;
  heading.append(title, output);
  const caption = document.createElement('p');
  caption.className = 'microcopy';
  caption.textContent = `${signal.freshness} · ${dirty ? 'Mapping and controls include staged edits; meters show confirmed device observations.' : 'Confirmed device observations.'}`;
  const strip = document.createElement('div');
  strip.className = 'signal-contributions';
  const parts = [
    ['Control input', 'Physical control / baseline', textValue(signal.baseline), signal.baseline, false, null],
    ['Reactive response', 'EF / ARG', signedValue(signal.reactive), signal.reactive, true, 'reactive'],
    ['Motion', 'LFO 1', signedValue(signal.lfos[0]), signal.lfos[0], true, 'lfo-0'],
    ['Motion', 'LFO 2', signedValue(signal.lfos[1]), signal.lfos[1], true, 'lfo-1'],
    ['Resolved output', 'MIDI value / mapping', textValue(signal.output), signal.output, false, null]
  ];
  parts.forEach(([label, technical, value, amount, bipolar, destination], partIndex) => {
    if (partIndex) {
      const operator = document.createElement('span');
      operator.className = 'signal-flow-operator';
      operator.setAttribute('aria-hidden', 'true');
      operator.textContent = partIndex === parts.length - 1 ? '=' : '+';
      strip.appendChild(operator);
    }
    const item = document.createElement(destination ? 'button' : 'div');
    item.className = `signal-contribution${destination ? ' signal-navigation' : ''}${partIndex === parts.length - 1 ? ' signal-output' : ''}`;
    if (destination) {
      item.type = 'button';
      const focusLabel = technical === 'EF / ARG' ? 'Reactive · EF / ARG' : technical;
      item.setAttribute('aria-label', `Focus ${focusLabel} controls`);
      item.addEventListener('click', () => onNavigate(destination));
    }
    const name = document.createElement('span');
    name.textContent = label;
    if (technical) {
      const detail = document.createElement('small');
      detail.className = 'signal-technical';
      detail.textContent = technical;
      name.append(' · ', detail);
    }
    const reading = document.createElement('strong');
    reading.textContent = value;
    const meter = document.createElement('meter');
    meter.min = bipolar ? -127 : 0;
    meter.max = 127;
    meter.value = amount ?? 0;
    meter.hidden = amount === null;
    meter.setAttribute('aria-label', `${technical ? `${label} · ${technical}` : label}: ${value}`);
    item.append(name, reading, meter);
    strip.appendChild(item);
  });
  const scrollCue = document.createElement('p');
  scrollCue.className = 'signal-scroll-cue';
  scrollCue.setAttribute('aria-hidden', 'true');
  scrollCue.textContent = 'Scroll for all contributions →';
  const route = document.createElement('p');
  route.className = 'microcopy signal-source';
  route.textContent = `Control input + Reactive response (EF / ARG) + Motion (LFO 1 / LFO 2) = Resolved output. ${signal.source}. ${signal.measured ? 'Measured deltas after clamping. Resolved output is a device observation; it does not confirm a MIDI message was sent. Select Reactive or an LFO to focus its controls.' : 'Separate contributions are not reported in the current snapshot.'}`;
  container.append(heading, caption, strip, scrollCue, route);
}
