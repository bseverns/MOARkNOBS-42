import { EF_FILTER_NAMES, SLOT_TYPE_NAMES, ARG_METHOD_NAMES } from '../lib/constants.js';
import { normalizeEfSlotTargets } from './patch_reconcile.js';

function clamp(value, min, max) {
  if (!Number.isFinite(value)) return min;
  return Math.min(max, Math.max(min, value));
}

function normalizeSlotEnvelope(slot) {
  const defaults = {
    index: -1,
    filter_index: 0,
    filter_name: EF_FILTER_NAMES[0],
    frequency: 1000,
    q: 0.707,
    oversample: 4,
    smoothing: 0.2,
    baseline: 0,
    gain: 1
  };
  const efSource = slot?.ef && typeof slot.ef === 'object' ? slot.ef : {};
  const ef = {
    index: defaults.index,
    filter_index: defaults.filter_index,
    filter_name: defaults.filter_name,
    frequency: defaults.frequency,
    q: defaults.q,
    oversample: defaults.oversample,
    smoothing: defaults.smoothing,
    baseline: defaults.baseline,
    gain: defaults.gain
  };
  if (efSource.index !== undefined) ef.index = efSource.index;
  if (efSource.filter_index !== undefined) ef.filter_index = efSource.filter_index;
  if (efSource.filter_name !== undefined) ef.filter_name = efSource.filter_name;
  if (efSource.frequency !== undefined) ef.frequency = efSource.frequency;
  if (efSource.q !== undefined) ef.q = efSource.q;
  if (efSource.oversample !== undefined) ef.oversample = efSource.oversample;
  if (efSource.smoothing !== undefined) ef.smoothing = efSource.smoothing;
  if (efSource.baseline !== undefined) ef.baseline = efSource.baseline;
  if (efSource.gain !== undefined) ef.gain = efSource.gain;
  const index = Number.isFinite(Number(slot?.efIndex))
    ? Number(slot.efIndex)
    : Number.isFinite(Number(ef.index))
      ? Number(ef.index)
      : defaults.index;
  ef.index = index;
  const resolvedFilterIndex = Number.isFinite(Number(ef.filter_index))
    ? Number(ef.filter_index)
    : null;
  if (resolvedFilterIndex === null && typeof ef.filter_name === 'string') {
    const idx = EF_FILTER_NAMES.indexOf(ef.filter_name);
    ef.filter_index = idx >= 0 ? idx : defaults.filter_index;
  } else if (resolvedFilterIndex !== null) {
    ef.filter_index = clamp(resolvedFilterIndex, 0, EF_FILTER_NAMES.length - 1);
  } else {
    ef.filter_index = defaults.filter_index;
  }
  if (!ef.filter_name || typeof ef.filter_name !== 'string') {
    ef.filter_name = EF_FILTER_NAMES[ef.filter_index] || defaults.filter_name;
  }
  ef.frequency = Number.isFinite(Number(ef.frequency))
    ? Math.max(0, Number(ef.frequency))
    : defaults.frequency;
  ef.q = Number.isFinite(Number(ef.q)) ? Math.max(0, Number(ef.q)) : defaults.q;
  ef.oversample = clamp(Math.round(Number(ef.oversample) || defaults.oversample), 1, 32);
  const smoothing = Number(ef.smoothing);
  ef.smoothing = Number.isFinite(smoothing) ? clamp(smoothing, 0, 1) : defaults.smoothing;
  ef.baseline = Number.isFinite(Number(ef.baseline)) ? Number(ef.baseline) : defaults.baseline;
  ef.gain = Number.isFinite(Number(ef.gain)) ? Number(ef.gain) : defaults.gain;
  return ef;
}

function normalizeSlotArg(slot, efLimit = 6) {
  const defaults = {
    enabled: false,
    method: 0,
    method_name: ARG_METHOD_NAMES[0],
    sourceA: 0,
    sourceB: 1
  };
  const argSource = slot?.arg && typeof slot.arg === 'object' ? slot.arg : {};
  const arg = { ...defaults, ...argSource };
  arg.enabled = Boolean(arg.enabled);
  let methodIndex = Number.isFinite(Number(arg.method)) ? Number(arg.method) : -1;
  if (methodIndex < 0 && typeof arg.method_name === 'string') {
    methodIndex = ARG_METHOD_NAMES.indexOf(arg.method_name);
  }
  if (methodIndex < 0) methodIndex = defaults.method;
  arg.method = clamp(Math.round(methodIndex), 0, ARG_METHOD_NAMES.length - 1);
  arg.method_name = ARG_METHOD_NAMES[arg.method] || defaults.method_name;
  const sourceA = Number.isFinite(Number(arg.sourceA))
    ? Math.round(Number(arg.sourceA))
    : defaults.sourceA;
  const sourceB = Number.isFinite(Number(arg.sourceB))
    ? Math.round(Number(arg.sourceB))
    : defaults.sourceB;
  arg.sourceA = sourceA;
  arg.sourceB = sourceB;
  return arg;
}

function normalizeSlotConfig(slot, efLimit = 6) {
  const source = slot && typeof slot === 'object' ? slot : {};
  const efMax = Math.max(
    -1,
    Math.round(Number.isFinite(Number(efLimit)) ? Number(efLimit) - 1 : 5)
  );
  const typeCandidate =
    typeof source.type === 'string'
      ? source.type
      : typeof source.type_name === 'string'
        ? source.type_name
        : null;
  const type = SLOT_TYPE_NAMES.includes(typeCandidate) ? typeCandidate : 'OFF';

  const midiChannelCandidate = Number(source.midiChannel ?? source.channel);
  const midiChannel = Number.isFinite(midiChannelCandidate)
    ? clamp(Math.round(midiChannelCandidate), 1, 16)
    : 1;

  const dataCandidate = Number(source.data1 ?? source.cc ?? source.note ?? source.value);
  const data1 = Number.isFinite(dataCandidate) ? clamp(Math.round(dataCandidate), 0, 127) : 0;

  const efIndexCandidate = Number(source.efIndex ?? source.ef_index ?? source.ef?.index);
  const efIndex = Number.isFinite(efIndexCandidate)
    ? clamp(Math.round(efIndexCandidate), -1, Math.max(-1, efMax))
    : -1;

  const ef = normalizeSlotEnvelope(source);
  ef.index = efIndex;

  const activeCandidate = source.active ?? source.enabled;
  const active = typeof activeCandidate === 'boolean' ? activeCandidate : Boolean(activeCandidate);

  const arg = normalizeSlotArg(source, efLimit);

  const normalized = { type, midiChannel, data1, efIndex, ef, active, arg };

  let sysexTemplate = source.sysexTemplate ?? source.sysex_template;
  if (typeof sysexTemplate === 'string') {
    sysexTemplate = sysexTemplate.trim().slice(0, 128);
    normalized.sysexTemplate = sysexTemplate;
  }

  return normalized;
}

export function normalizeConfig(config, manifest = {}) {
  if (!config || typeof config !== 'object') return config;
  const slotCount = Number.isFinite(Number(manifest.slot_count))
    ? Number(manifest.slot_count)
    : Array.isArray(config.slots)
      ? config.slots.length
      : 0;
  const manifestEf = Number.isFinite(Number(manifest.envelope_count))
    ? Number(manifest.envelope_count)
    : null;
  const configEf = Array.isArray(config.efSlots) ? config.efSlots.length : null;
  const followerEf = Array.isArray(config.envelopes?.followers)
    ? config.envelopes.followers.length
    : null;
  const efCount = manifestEf ?? configEf ?? followerEf ?? 0;

  const slots = Array.from({ length: slotCount }, (_, idx) =>
    normalizeSlotConfig(config.slots?.[idx], efCount)
  );

  let efSlots;
  if (Array.isArray(config.efSlots)) {
    efSlots = Array.from({ length: efCount }, (_, idx) => {
      const entry = config.efSlots[idx];
      return { slots: normalizeEfSlotTargets(entry, slotCount) };
    });
  } else {
    const derived = Array.from({ length: efCount }, () => new Set());
    const routing = Array.isArray(config.envelopes?.routing) ? config.envelopes.routing : [];
    routing.forEach((value, potIndex) => {
      const follower = Number(value);
      if (!Number.isFinite(follower)) return;
      if (follower < 0 || follower >= derived.length) return;
      const capped = clamp(potIndex, 0, Math.max(0, slotCount - 1));
      derived[follower].add(capped);
    });
    slots.forEach((slot, slotIndex) => {
      const raw = Number.isFinite(Number(slot?.efIndex))
        ? Number(slot.efIndex)
        : Number.isFinite(Number(slot?.ef?.index))
          ? Number(slot.ef.index)
          : -1;
      if (!Number.isFinite(raw) || raw < 0 || raw >= derived.length) return;
      derived[raw].add(clamp(slotIndex, 0, Math.max(0, slotCount - 1)));
    });
    efSlots = derived.map((slotsForFollower) => ({
      slots: Array.from(slotsForFollower).sort((a, b) => a - b)
    }));
  }

  let legacyLedColor = null;
  if (Array.isArray(config.ledColors) && config.ledColors.length) {
    const swatch = config.ledColors.find(
      (entry) => typeof entry?.color === 'string' && /^#([0-9a-fA-F]{6})$/.test(entry.color)
    );
    if (swatch) legacyLedColor = swatch.color.toUpperCase();
  }
  const env = config.envelopes && typeof config.envelopes === 'object' ? config.envelopes : {};

  const filterSource = config.filter && typeof config.filter === 'object' ? config.filter : {};
  const envFilter = env.filter && typeof env.filter === 'object' ? env.filter : {};
  const filter = {};
  const freqCandidate = Number(filterSource.freq ?? filterSource.frequency);
  if (Number.isFinite(freqCandidate)) {
    filter.freq = freqCandidate;
  } else {
    const envFreq = Number(envFilter.frequency ?? envFilter.freq);
    if (Number.isFinite(envFreq)) filter.freq = envFreq;
  }
  if (!Number.isFinite(filter.freq)) filter.freq = 20;

  const qCandidate = Number(filterSource.q);
  if (Number.isFinite(qCandidate)) {
    filter.q = qCandidate;
  } else {
    const envQ = Number(envFilter.q);
    if (Number.isFinite(envQ)) filter.q = envQ;
  }
  if (!Number.isFinite(filter.q)) filter.q = 1;

  if (typeof filterSource.type === 'string' && EF_FILTER_NAMES.includes(filterSource.type)) {
    filter.type = filterSource.type;
  } else if (typeof envFilter.type === 'string' && EF_FILTER_NAMES.includes(envFilter.type)) {
    filter.type = envFilter.type;
  } else {
    const followerFilter = env.followers?.find(
      (entry) => typeof entry?.filter === 'string'
    )?.filter;
    const slotFilter = slots.find((slot) => typeof slot?.ef?.filter_name === 'string')?.ef
      ?.filter_name;
    const derivedFilter = followerFilter || slotFilter;
    filter.type =
      typeof derivedFilter === 'string' && EF_FILTER_NAMES.includes(derivedFilter)
        ? derivedFilter
        : 'LINEAR';
  }

  const argSource = config.arg && typeof config.arg === 'object' ? config.arg : {};
  const pair = env.arg_pair && typeof env.arg_pair === 'object' ? env.arg_pair : {};
  const readNumber = (value, fallback) => {
    const num = Number(value);
    return Number.isFinite(num) ? num : fallback;
  };

  let methodName = null;
  if (typeof argSource.method === 'string' && ARG_METHOD_NAMES.includes(argSource.method)) {
    methodName = argSource.method;
  } else if (
    typeof argSource.method_name === 'string' &&
    ARG_METHOD_NAMES.includes(argSource.method_name)
  ) {
    methodName = argSource.method_name;
  } else if (Number.isFinite(Number(argSource.method))) {
    const idx = Math.max(
      0,
      Math.min(ARG_METHOD_NAMES.length - 1, Math.round(Number(argSource.method)))
    );
    methodName = ARG_METHOD_NAMES[idx];
  } else if (
    typeof env.arg_method_name === 'string' &&
    ARG_METHOD_NAMES.includes(env.arg_method_name)
  ) {
    methodName = env.arg_method_name;
  } else if (Number.isFinite(Number(env.arg_method))) {
    const idx = Math.max(
      0,
      Math.min(ARG_METHOD_NAMES.length - 1, Math.round(Number(env.arg_method)))
    );
    methodName = ARG_METHOD_NAMES[idx];
  } else {
    methodName = ARG_METHOD_NAMES[0];
  }

  let enabled;
  if (typeof argSource.enable === 'boolean') enabled = argSource.enable;
  else if (typeof argSource.enabled === 'boolean') enabled = argSource.enabled;
  else if (typeof env.arg_enable === 'boolean') enabled = env.arg_enable;
  else if (typeof env.arg_enabled === 'boolean') enabled = env.arg_enabled;
  else enabled = true;

  let argA = readNumber(argSource.a ?? argSource.sourceA, undefined);
  if (argA === undefined) argA = readNumber(pair.a, 0);
  if (!Number.isFinite(argA)) argA = 0;

  let argB = readNumber(argSource.b ?? argSource.sourceB, undefined);
  if (argB === undefined) argB = readNumber(pair.b, Math.max(0, Math.min(1, efCount - 1)));
  if (!Number.isFinite(argB)) argB = Math.max(0, Math.min(1, efCount - 1));

  const arg = { method: methodName, a: argA, b: argB, enable: Boolean(enabled) };

  let led;
  if (config.led && typeof config.led === 'object') {
    const ledCandidate = { ...config.led };
    const brightness = Number(ledCandidate.brightness);
    const parsed = Number.isFinite(brightness) ? clamp(Math.round(brightness), 0, 255) : 0;
    let color = ledCandidate.color;
    if (typeof color !== 'string' || !/^#([0-9a-fA-F]{6})$/.test(color)) {
      if (typeof ledCandidate.hex === 'string' && /^#([0-9a-fA-F]{6})$/.test(ledCandidate.hex)) {
        color = ledCandidate.hex.toUpperCase();
      } else if (
        ledCandidate.rgb &&
        Number.isFinite(ledCandidate.rgb.r) &&
        Number.isFinite(ledCandidate.rgb.g) &&
        Number.isFinite(ledCandidate.rgb.b)
      ) {
        color = `#${[ledCandidate.rgb.r, ledCandidate.rgb.g, ledCandidate.rgb.b]
          .map((value) => clamp(Math.round(value), 0, 255).toString(16).padStart(2, '0'))
          .join('')}`.toUpperCase();
      } else {
        color = '#000000';
      }
    }
    led = { brightness: parsed, color: color.toUpperCase?.() || '#000000' };
  }

  if (!led) {
    led = { brightness: 0, color: '#000000' };
  }

  if (legacyLedColor) {
    if (!led || typeof led !== 'object') {
      led = { brightness: 0, color: legacyLedColor };
    } else if (typeof led.color !== 'string' || !/^#([0-9a-fA-F]{6})$/.test(led.color)) {
      led = { ...led, color: legacyLedColor };
    }
  }

  if (led && typeof led === 'object') {
    let ledModeValue = null;
    if (config.led && typeof config.led === 'object' && typeof config.led.mode === 'string') {
      ledModeValue = config.led.mode.toUpperCase();
    } else if (env.led && typeof env.led === 'object' && typeof env.led.mode === 'string') {
      ledModeValue = env.led.mode.toUpperCase();
    }
    if (!ledModeValue) {
      ledModeValue = 'STATIC';
    }
    led.mode = ledModeValue;
  }

  let envelopeMode = null;
  if (typeof config.envelopeMode === 'string') envelopeMode = config.envelopeMode;
  else if (typeof env.mode_name === 'string') envelopeMode = env.mode_name;
  if (!['LINEAR', 'EXPONENTIAL', 'LOG'].includes(envelopeMode)) {
    envelopeMode = null;
  }

  const normalized = { slots, efSlots, filter, arg, led };
  if (envelopeMode) normalized.envelopeMode = envelopeMode;
  return normalized;
}
