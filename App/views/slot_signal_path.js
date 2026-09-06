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
  const efIndex = numberOrNull(slot.efIndex ?? slot.ef?.index);
  return {
    freshness,
    baseline: connected ? numberOrNull(measured ? entry.baseline : telemetry.slots?.[index]) : null,
    output: connected
      ? numberOrNull(measured ? entry.output : telemetry.slotOutputs?.[index])
      : null,
    reactive: measured ? Number(entry.ef) : null,
    lfos: measured ? entry.lfos.map(Number) : [null, null],
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
  { slot, liveSlot, telemetry, index, connected, dirty } = {}
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
    ['Hand / base', textValue(signal.baseline), signal.baseline, false],
    ['Reactive · EF / ARG', signedValue(signal.reactive), signal.reactive, true],
    ['Motion · LFO 1', signedValue(signal.lfos[0]), signal.lfos[0], true],
    ['Motion · LFO 2', signedValue(signal.lfos[1]), signal.lfos[1], true],
    ['Resolved output', textValue(signal.output), signal.output, false]
  ];
  parts.forEach(([label, value, amount, bipolar]) => {
    const item = document.createElement('div');
    item.className = 'signal-contribution';
    const name = document.createElement('span');
    name.textContent = label;
    const reading = document.createElement('strong');
    reading.textContent = value;
    const meter = document.createElement('meter');
    meter.min = bipolar ? -127 : 0;
    meter.max = 127;
    meter.value = amount ?? 0;
    meter.hidden = amount === null;
    meter.setAttribute('aria-label', `${label}: ${value}`);
    item.append(name, reading, meter);
    strip.appendChild(item);
  });
  const route = document.createElement('p');
  route.className = 'microcopy signal-source';
  route.textContent = `Confirmed path: ${signal.source} → LFO 1 → LFO 2 → output. ${signal.measured ? 'Values are measured deltas after clamping.' : 'Separate contributions are not reported in the current snapshot.'}`;
  container.append(heading, caption, strip, route);
}
