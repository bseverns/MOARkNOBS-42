import Ajv from './lib/mini-ajv.js';
import addFormats from './lib/add-formats.js';

const DEFAULT_DEBOUNCE = 24;
const TELEMETRY_FRAME_MS = 16;
const RPC_THROTTLE_INTERVAL_MS = 1000 / 120;
const RPC_TIMEOUT_MS = 3000;
const MACRO_COMMAND_TIMEOUT_MS = 6000;
const MACRO_RESPONSE_KEYS = {
  SAVE_MACRO_SLOT: 'macro_saved',
  RECALL_MACRO_SLOT: 'macro_recalled'
};
const SCENE_COMMAND_TIMEOUT_MS = 6000;
const SCENE_RESPONSE_KEYS = {
  SAVE_SCENE: 'scene_saved',
  RECALL_SCENE: 'scene_recalled',
  GET_SCENES: 'scenes'
};
const STORAGE_KEY = 'moarknobs:last-port';
const STATE_STORAGE_KEY = 'moarknobs:last-state';
const EF_FILTER_NAMES = [
  'LINEAR',
  'OPPOSITE_LINEAR',
  'EXPONENTIAL',
  'RANDOM',
  'LOWPASS',
  'HIGHPASS',
  'BANDPASS'
];
const SLOT_TYPE_NAMES = [
  'OFF',
  'CC',
  'Note',
  'PitchBend',
  'ProgramChange',
  'Aftertouch',
  'ModWheel',
  'NRPN',
  'RPN',
  'SysEx'
];
const ARG_METHOD_NAMES = [
  'PLUS',
  'MIN',
  'PECK',
  'SHAV',
  'SQAR',
  'BABS',
  'TABS',
  'MULT',
  'DIVI',
  'AVG',
  'XABS',
  'MAXX',
  'MINN',
  'XORR'
];

function makeEmitter() {
  const listeners = new Map();
  return {
    on(event, handler) {
      const set = listeners.get(event) ?? new Set();
      set.add(handler);
      listeners.set(event, set);
      return () => set.delete(handler);
    },
    emit(event, payload) {
      const set = listeners.get(event);
      if (!set) return;
      for (const handler of [...set]) {
        try {
          handler(payload);
        } catch (err) {
          console.error('runtime listener error', event, err);
        }
      }
    }
  };
}

function encoder() {
  return new TextEncoder();
}

function decoder() {
  return new TextDecoder();
}

async function digest(message) {
  const bytes = typeof message === 'string' ? new TextEncoder().encode(message) : message;
  const hash = await crypto.subtle.digest('SHA-256', bytes);
  const view = new DataView(hash);
  let hex = '';
  for (let i = 0; i < view.byteLength; i += 4) {
    const value = view.getUint32(i);
    hex += value.toString(16).padStart(8, '0');
  }
  return hex;
}

function clone(value) {
  if (value === undefined) return value;
  if (typeof structuredClone === 'function') return structuredClone(value);
  return JSON.parse(JSON.stringify(value));
}

function shallowDiff(before, after, basePath = '') {
  const changes = [];
  if (before === after) return changes;
  if (typeof before !== 'object' || typeof after !== 'object' || !before || !after) {
    changes.push({ path: basePath || '.', before, after });
    return changes;
  }
  const keys = new Set([...Object.keys(before), ...Object.keys(after)]);
  for (const key of keys) {
    const nextPath = basePath ? `${basePath}.${key}` : key;
    const prev = before[key];
    const next = after[key];
    if (typeof prev === 'object' && typeof next === 'object') {
      changes.push(...shallowDiff(prev ?? null, next ?? null, nextPath));
    } else if (prev !== next) {
      changes.push({ path: nextPath, before: prev, after: next });
    }
  }
  return changes;
}

function shallowEqual(a, b) {
  if (a === b) return true;
  if (typeof a !== 'object' || typeof b !== 'object' || !a || !b) return false;
  const keys = new Set([...Object.keys(a), ...Object.keys(b)]);
  for (const key of keys) {
    if (a[key] !== b[key]) return false;
  }
  return true;
}

function parseSegment(segment) {
  const idx = Number(segment);
  return Number.isInteger(idx) && String(idx) === segment ? idx : segment;
}

function setNestedValue(target, path, value) {
  if (!path) return;
  const segments = path.split('.').map(parseSegment);
  let cursor = target;
  for (let i = 0; i < segments.length; i += 1) {
    const segment = segments[i];
    if (i === segments.length - 1) {
      cursor[segment] = value;
      break;
    }
    const nextSegment = segments[i + 1];
    if (cursor[segment] === undefined || cursor[segment] === null) {
      cursor[segment] = typeof nextSegment === 'number' ? [] : {};
    }
    cursor = cursor[segment];
  }
}

function clamp(value, min, max) {
  if (!Number.isFinite(value)) return min;
  return Math.min(max, Math.max(min, value));
}

function createThrottle(fn, delay = DEFAULT_DEBOUNCE) {
  let timer = null;
  let pendingArgs = null;
  let lastRun = 0;
  return (...args) => {
    pendingArgs = args;
    const now = performance.now();
    const invoke = () => {
      lastRun = performance.now();
      timer = null;
      const runArgs = pendingArgs;
      pendingArgs = null;
      fn(...runArgs);
    };
    if (!timer) {
      const since = now - lastRun;
      const wait = since >= delay ? 0 : delay - since;
      timer = setTimeout(invoke, wait);
    }
  };
}

function coerceIndex(value) {
  const num = Number(value);
  return Number.isInteger(num) && num >= 0 ? num : null;
}

function extractSlotIndex(payload, fallback) {
  const candidates = [payload?.index, payload?.slot, payload?.slot_index, payload?.slotIndex, fallback];
  for (const candidate of candidates) {
    const idx = coerceIndex(candidate);
    if (idx !== null) return idx;
  }
  return null;
}

function normalizeSlotPatchEntry(entry) {
  if (!entry || typeof entry !== 'object') return null;
  const index = coerceIndex(entry.index ?? entry.id ?? entry.slot);
  if (index === null) return null;
  const fields = {};
  if (entry.type !== undefined) fields.type = entry.type;
  else if (entry.type_name !== undefined) fields.type = entry.type_name;
  if (entry.type_code !== undefined) fields.typeCode = entry.type_code;
  const midiChannel = entry.midiChannel ?? entry.channel;
  if (midiChannel !== undefined) {
    const value = Number(midiChannel);
    if (Number.isFinite(value)) fields.midiChannel = value;
  }
  if (entry.data1 !== undefined) {
    const value = Number(entry.data1);
    if (Number.isFinite(value)) fields.data1 = value;
  }
  const efIndexValue =
    entry.efIndex ?? entry.ef_index ?? (entry.ef && typeof entry.ef === 'object' ? entry.ef.index : undefined);
  if (efIndexValue !== undefined) {
    const value = Number(efIndexValue);
    if (Number.isFinite(value)) fields.efIndex = value;
  }
  if (entry.ef && typeof entry.ef === 'object') {
    const ef = {};
    const coerceNumber = (value) => {
      const num = Number(value);
      return Number.isFinite(num) ? num : null;
    };
    const pushNumber = (sourceKey, targetKey = sourceKey) => {
      if (entry.ef[sourceKey] === undefined) return;
      const num = coerceNumber(entry.ef[sourceKey]);
      if (num === null) return;
      ef[targetKey] = num;
    };
    if (entry.ef.filter_name !== undefined) {
      ef.filter_name = String(entry.ef.filter_name);
    }
    pushNumber('index');
    pushNumber('filter_index');
    pushNumber('frequency');
    pushNumber('q');
    pushNumber('oversample');
    pushNumber('smoothing');
    pushNumber('baseline');
    pushNumber('gain');
    if (Object.keys(ef).length) {
      fields.ef = ef;
    }
  }
  if (entry.arg && typeof entry.arg === 'object') {
    const arg = {};
    if (entry.arg.enabled !== undefined) {
      const raw = entry.arg.enabled;
      if (typeof raw === 'string') {
        arg.enabled = raw === 'true' || raw === '1';
      } else if (typeof raw === 'number') {
        arg.enabled = raw !== 0;
      } else {
        arg.enabled = Boolean(raw);
      }
    }
    if (entry.arg.method !== undefined) {
      const methodValue = Number(entry.arg.method);
      if (Number.isFinite(methodValue)) arg.method = methodValue;
    }
    if (entry.arg.method_name !== undefined) {
      arg.method_name = String(entry.arg.method_name);
    }
    if (entry.arg.sourceA !== undefined) {
      const sourceA = Number(entry.arg.sourceA);
      if (Number.isFinite(sourceA)) arg.sourceA = sourceA;
    }
    if (entry.arg.sourceB !== undefined) {
      const sourceB = Number(entry.arg.sourceB);
      if (Number.isFinite(sourceB)) arg.sourceB = sourceB;
    }
    if (Object.keys(arg).length) {
      fields.arg = arg;
    }
  }
  if (entry.active !== undefined) fields.active = Boolean(entry.active);
  if (entry.takeover !== undefined) fields.takeover = Boolean(entry.takeover);
  if (entry.pot !== undefined) fields.pot = Boolean(entry.pot);
  if (entry.arpNote !== undefined) {
    const value = Number(entry.arpNote);
    if (Number.isFinite(value)) fields.arpNote = value;
  }
  if (entry.label !== undefined) fields.label = entry.label;
  return { index, fields };
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
  const ef = { ...defaults, ...efSource };
  const index = Number.isFinite(Number(slot?.efIndex))
    ? Number(slot.efIndex)
    : Number.isFinite(Number(ef.index))
    ? Number(ef.index)
    : defaults.index;
  ef.index = index;
  const resolvedFilterIndex = Number.isFinite(Number(ef.filter_index)) ? Number(ef.filter_index) : null;
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
  ef.frequency = Number.isFinite(Number(ef.frequency)) ? Math.max(0, Number(ef.frequency)) : defaults.frequency;
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
  const sourceA = Number.isFinite(Number(arg.sourceA)) ? Math.round(Number(arg.sourceA)) : defaults.sourceA;
  const sourceB = Number.isFinite(Number(arg.sourceB)) ? Math.round(Number(arg.sourceB)) : defaults.sourceB;
  arg.sourceA = sourceA;
  arg.sourceB = sourceB;
  return arg;
}

function normalizeSlotConfig(slot, efLimit = 6) {
  const source = slot && typeof slot === 'object' ? slot : {};
  const efMax = Math.max(-1, Math.round(Number.isFinite(Number(efLimit)) ? Number(efLimit) - 1 : 5));
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

  const potCandidate = source.pot;
  let pot;
  if (typeof potCandidate === 'boolean') pot = potCandidate;
  else if (typeof potCandidate === 'number') pot = potCandidate !== 0;
  else if (typeof potCandidate === 'string') pot = potCandidate === 'true' || potCandidate === '1';
  else pot = Boolean(potCandidate);

  const arg = normalizeSlotArg(source, efLimit);

  const normalized = { type, midiChannel, data1, efIndex, ef, active, pot, arg };

  const label = typeof source.label === 'string' ? source.label : undefined;
  if (label !== undefined) normalized.label = label;

  const takeoverCandidate = source.takeover;
  if (typeof takeoverCandidate === 'boolean') normalized.takeover = takeoverCandidate;
  else if (typeof takeoverCandidate === 'number') normalized.takeover = takeoverCandidate !== 0;

  let sysexTemplate = source.sysexTemplate ?? source.sysex_template;
  if (typeof sysexTemplate === 'string') {
    sysexTemplate = sysexTemplate.trim().slice(0, 128);
    normalized.sysexTemplate = sysexTemplate;
  }

  return normalized;
}

function normalizeConfig(config, manifest = {}) {
  if (!config || typeof config !== 'object') return config;
  const slotCount = Number.isFinite(Number(manifest.slot_count))
    ? Number(manifest.slot_count)
    : Array.isArray(config.slots)
    ? config.slots.length
    : 0;
  const manifestEf = Number.isFinite(Number(manifest.envelope_count)) ? Number(manifest.envelope_count) : null;
  const configEf = Array.isArray(config.efSlots) ? config.efSlots.length : null;
  const followerEf = Array.isArray(config.envelopes?.followers) ? config.envelopes.followers.length : null;
  const efCount = manifestEf ?? configEf ?? followerEf ?? 0;

  const slots = Array.from({ length: slotCount }, (_, idx) => normalizeSlotConfig(config.slots?.[idx], efCount));

  let efSlots;
  if (Array.isArray(config.efSlots)) {
    efSlots = Array.from({ length: efCount }, (_, idx) => {
      const entry = config.efSlots[idx];
      const slotIndex = entry && Number.isFinite(Number(entry.slot)) ? Number(entry.slot) : -1;
      if (slotIndex < 0) return { slot: -1 };
      const capped = clamp(slotIndex, 0, Math.max(0, slotCount - 1));
      return { slot: capped };
    });
  } else {
    const derived = Array.from({ length: efCount }, () => ({ slot: -1 }));
    const routing = Array.isArray(config.envelopes?.routing) ? config.envelopes.routing : [];
    routing.forEach((value, potIndex) => {
      const follower = Number(value);
      if (!Number.isFinite(follower)) return;
      if (follower < 0 || follower >= derived.length) return;
      if (derived[follower].slot !== -1) return;
      const capped = clamp(potIndex, 0, Math.max(0, slotCount - 1));
      derived[follower] = { slot: capped };
    });
    slots.forEach((slot, slotIndex) => {
      const raw = Number.isFinite(Number(slot?.efIndex))
        ? Number(slot.efIndex)
        : Number.isFinite(Number(slot?.ef?.index))
        ? Number(slot.ef.index)
        : -1;
      if (!Number.isFinite(raw) || raw < 0 || raw >= derived.length) return;
      if (derived[raw].slot !== -1) return;
      derived[raw] = { slot: clamp(slotIndex, 0, Math.max(0, slotCount - 1)) };
    });
    efSlots = derived;
  }

  let legacyLedColor = null;
  if (Array.isArray(config.ledColors) && config.ledColors.length) {
    const swatch = config.ledColors.find((entry) => typeof entry?.color === 'string' && /^#([0-9a-fA-F]{6})$/.test(entry.color));
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
    const followerFilter = env.followers?.find((entry) => typeof entry?.filter === 'string')?.filter;
    const slotFilter = slots.find((slot) => typeof slot?.ef?.filter_name === 'string')?.ef?.filter_name;
    const derivedFilter = followerFilter || slotFilter;
    filter.type = typeof derivedFilter === 'string' && EF_FILTER_NAMES.includes(derivedFilter) ? derivedFilter : 'LINEAR';
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
  } else if (typeof argSource.method_name === 'string' && ARG_METHOD_NAMES.includes(argSource.method_name)) {
    methodName = argSource.method_name;
  } else if (Number.isFinite(Number(argSource.method))) {
    const idx = Math.max(0, Math.min(ARG_METHOD_NAMES.length - 1, Math.round(Number(argSource.method))));
    methodName = ARG_METHOD_NAMES[idx];
  } else if (typeof env.arg_method_name === 'string' && ARG_METHOD_NAMES.includes(env.arg_method_name)) {
    methodName = env.arg_method_name;
  } else if (Number.isFinite(Number(env.arg_method))) {
    const idx = Math.max(0, Math.min(ARG_METHOD_NAMES.length - 1, Math.round(Number(env.arg_method))));
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

  const normalized = { slots, efSlots, filter, arg, led };
  if (envelopeMode) normalized.envelopeMode = envelopeMode;
  return normalized;
}

function applyEfSlotPatch(target, patch) {
  if (!Array.isArray(target)) target = [];
  const index = coerceIndex(patch.index);
  const slotValue = patch.slot ?? patch.value ?? patch.target;
  if (index === null || slotValue === undefined) return target;
  const value = Number(slotValue);
  if (!Number.isFinite(value)) return target;
  const existing = target[index];
  if (existing && Number(existing.slot) === value) return target;
  const copy = [...target];
  copy[index] = { ...(existing ?? {}), slot: value };
  return copy;
}

function createTransportPort(port, options = {}) {
  const textEncoder = encoder();
  const textDecoder = decoder();
  let reader;
  let writer;
  let active = true;
  const lineQueue = [];
  const waiters = [];

  async function open() {
    await port.open({ baudRate: 115200, ...options });
    const decoderStream = new TextDecoderStream();
    port.readable.pipeTo(decoderStream.writable);
    reader = decoderStream.readable.getReader();
    writer = port.writable.getWriter();
    readLoop();
  }

  async function readLoop() {
    let buffer = '';
    try {
      while (active) {
        const { value, done } = await reader.read();
        if (done) break;
        buffer += value;
        let idx;
        while ((idx = buffer.indexOf('\n')) >= 0) {
          const line = buffer.slice(0, idx).trim();
          buffer = buffer.slice(idx + 1);
          if (!line) continue;
          if (waiters.length) waiters.shift()(line);
          else lineQueue.push(line);
        }
      }
    } catch (err) {
      console.error('transport read error', err);
    } finally {
      active = false;
    }
  }

  async function writeLine(line) {
    if (!writer) throw new Error('Writer unavailable');
    await writer.write(textEncoder.encode(line + '\n'));
  }

  function nextLine() {
    if (lineQueue.length) return Promise.resolve(lineQueue.shift());
    return new Promise((resolve) => waiters.push(resolve));
  }

  async function close() {
    active = false;
    try {
      reader?.cancel();
    } catch (err) {
      console.debug('reader cancel', err);
    }
    try {
      writer?.close();
    } catch (err) {
      console.debug('writer close', err);
    }
    try {
      await port.close();
    } catch (err) {
      console.debug('port close', err);
    }
  }

  return { open, writeLine, nextLine, close, rawPort: port };
}

function createSimulator() {
  let opened = false;
  let index = 0;
  const lines = [];
  let resolver;

  const manifest = {
    fw_version: 'sim-fw',
    git_sha: 'deadbeef',
    build_time: new Date().toISOString(),
    schema_version: 5,
    slot_count: 42,
    pot_count: 42,
    envelope_count: 6,
    arg_method_count: ARG_METHOD_NAMES.length,
    led_count: 51,
    free_ram: 48000,
    free_flash: 512000
  };

  let config = {
    fw_version: manifest.fw_version,
    schema_version: manifest.schema_version,
    pots: Array.from({ length: manifest.pot_count }, (_, idx) => ({
      index: idx,
      channel: (idx % 16) + 1,
      cc: (idx * 7) % 128
    })),
    slots: Array.from({ length: manifest.slot_count }, (_, idx) => {
      const efIndex = idx % manifest.envelope_count;
      const filterIndex = idx % EF_FILTER_NAMES.length;
      const argMethod = idx % ARG_METHOD_NAMES.length;
      return {
        index: idx,
        type: 'CC',
        type_name: 'CC',
        channel: (idx % 16) + 1,
        data1: (idx % 120) + 1,
        ef_index: efIndex,
        ef: {
          index: efIndex,
          filter_index: filterIndex,
          filter_name: EF_FILTER_NAMES[filterIndex],
          frequency: 400 + (idx % 8) * 50,
          q: 0.6 + (idx % 5) * 0.1,
          oversample: 4,
          smoothing: 0.2 + (idx % 3) * 0.1,
          baseline: 0,
          gain: 1
        },
        ef_payload: {
          type: EF_FILTER_NAMES[filterIndex],
          freq: 400 + (idx % 8) * 50,
          q: 0.6 + (idx % 5) * 0.1
        },
        arg: {
          enabled: idx % 3 === 0,
          method: argMethod,
          method_name: ARG_METHOD_NAMES[argMethod],
          sourceA: efIndex,
          sourceB: (efIndex + 1) % manifest.envelope_count
        },
        active: idx % 2 === 0,
        pot: true,
        takeover: idx % 5 === 0,
        arp_note: (idx * 3) % 128,
        label: `Slot ${idx + 1}`,
        sysexTemplate: ''
      };
    }),
    envelopes: {
      routing: Array.from({ length: manifest.pot_count }, (_, idx) => idx % manifest.envelope_count),
      followers: Array.from({ length: manifest.envelope_count }, (_, idx) => ({
        index: idx,
        active: idx % 2 === 0,
        filter: EF_FILTER_NAMES[idx % EF_FILTER_NAMES.length],
        baseline: 0,
        oversample: 4,
        smoothing: 0.25
      })),
      mode: 0,
      mode_name: 'LINEAR',
      arg_method: 0,
      arg_method_name: 'PLUS',
      arg_enable: true,
      arg_pair: { a: 0, b: 1 },
      filter: { frequency: 800, q: 1 }
    },
    led: {
      brightness: 64,
      hex: '#ff00ff',
      rgb: { r: 255, g: 0, b: 255 }
    }
  };
  let macroSnapshot = null;
  const profileSlots = Array.from({ length: 4 }, () => clone(config));
  const defaultProfile = clone(config);

  const telemetry = () => ({
    slots: Array.from({ length: manifest.slot_count }, () => Math.floor(Math.random() * 127)),
    slotArgs: Array.from({ length: manifest.slot_count }, (_, idx) => ({
      enabled: idx % 2 === 0,
      method: idx % ARG_METHOD_NAMES.length,
      method_name: ARG_METHOD_NAMES[idx % ARG_METHOD_NAMES.length],
      sourceA: idx % manifest.envelope_count,
      sourceB: (idx + 1) % manifest.envelope_count
    })),
    envelopes: Array.from({ length: manifest.envelope_count }, () => Math.floor(Math.random() * 127)),
    currentSlot: index++ % manifest.slot_count,
    argPair: [0, 1],
    argEnabled: true,
    efStatus: Array.from({ length: manifest.envelope_count }, (_, idx) => (idx % 2 === 0 ? 1 : 0)),
    diagnostics: {
      loop_max_us: 702,
      loop_last_us: 512,
      midi_isr_max_us: 320,
      midi_isr_last_us: 110,
      midi_drops: 0,
      uart_overruns: 0,
      loop_overruns: 0,
      midi_task_overruns: 0
    }
  });

  async function open() {
    opened = true;
  }

  function pushLine(payload) {
    lines.push(JSON.stringify(payload));
    if (resolver) {
      const fn = resolver;
      resolver = null;
      fn(lines.shift());
    }
  }

  function handleSimulatorMacroCommand(command) {
    if (!command) return false;
    if (command === 'SAVE_MACRO_SLOT') {
      macroSnapshot = clone(config);
      pushLine({ macro_saved: true, macro_available: true });
      return true;
    }
    if (command === 'RECALL_MACRO_SLOT') {
      const hasSnapshot = Boolean(macroSnapshot);
      if (hasSnapshot) {
        config = clone(macroSnapshot);
      }
      pushLine({ macro_recalled: hasSnapshot, macro_available: hasSnapshot });
      return true;
    }
    return false;
  }

  async function writeLine(line) {
    if (!opened) throw new Error('simulator closed');
    const trimmed = line.trim();
    if (!trimmed) return;
    if (handleSimulatorMacroCommand(trimmed)) return;
    let request;
    try {
      request = JSON.parse(trimmed);
    } catch (err) {
      pushLine({ error: { message: err.message } });
      return;
    }
    const rpc = request.rpc ?? request.method;
    const respond = (result) => {
      if (request.id === undefined) return;
      pushLine({ id: request.id, result });
    };
    const clampSlot = (value) => {
      const idx = Number.isFinite(Number(value)) ? Number(value) : 0;
      return Math.max(0, Math.min(profileSlots.length - 1, Math.floor(idx)));
    };
    switch (rpc) {
      case 'hello':
        respond({ message: 'hello' });
        break;
      case 'get_manifest':
        respond({ manifest });
        break;
      case 'get_config':
        respond({ config });
        break;
      case 'set_config':
        if (request.config && typeof request.config === 'object') {
          config = { ...config, ...request.config };
        }
        respond({ checksum: request.checksum ?? 'sim-checksum' });
        break;
      case 'set_param':
        if (typeof request.path !== 'string' || !request.path.length) {
          if (request.id !== undefined) {
            pushLine({ id: request.id, error: { message: 'set_param requires path' } });
          }
          break;
        }
        setNestedValue(config, request.path, request.value);
        respond({ ok: true, path: request.path, value: request.value });
        break;
      case 'save_profile': {
        const slot = clampSlot(request.slot ?? request.id ?? 0);
        profileSlots[slot] = clone(config);
        respond({ slot, saved: true });
        break;
      }
      case 'load_profile': {
        const slot = clampSlot(request.slot ?? request.id ?? 0);
        const loaded = profileSlots[slot] ?? clone(defaultProfile);
        config = clone(loaded);
        respond({ slot, config: clone(config) });
        break;
      }
      case 'reset_profile': {
        const slot = clampSlot(request.slot ?? request.id ?? 0);
        profileSlots[slot] = clone(defaultProfile);
        config = clone(defaultProfile);
        respond({ slot, config: clone(config) });
        break;
      }
      case 'hang':
        break;
      default:
        if (request.id !== undefined) {
          pushLine({ id: request.id, error: { message: 'Unsupported RPC' } });
        }
        break;
    }
  }

  function nextLine() {
    if (!opened) return Promise.reject(new Error('simulator closed'));
    if (lines.length) return Promise.resolve(lines.shift());
    return new Promise((resolve) => {
      resolver = resolve;
      setTimeout(() => {
        if (!resolver) return;
        pushLine({ type: 'telemetry', ...telemetry() });
      }, TELEMETRY_FRAME_MS * 4);
    });
  }

  async function close() {
    opened = false;
  }

  return { open, writeLine, nextLine, close, rawPort: { getInfo: () => ({ usbVendorId: 0xfeed, usbProductId: 0xbeef }) } };
}

function createWebSocketTransport(url) {
  let socket = null;
  let queue = [];
  let resolver = null;
  let buffer = '';
  let closed = false;
  const decoder = typeof TextDecoder === 'function' ? new TextDecoder() : null;

  const enqueueLine = (line) => {
    queue.push(line);
    if (resolver) {
      const pending = resolver;
      resolver = null;
      const next = queue.shift();
      pending.resolve(next);
    }
  };

  const flushBuffer = () => {
    if (!buffer) return;
    const trimmed = buffer.trim();
    buffer = '';
    if (trimmed) enqueueLine(trimmed);
  };

  const handleMessage = (event) => {
    if (!event || closed) return;
    const data = event.data;
    const text = typeof data === 'string' ? data : decoder?.decode(data) ?? '';
    if (!text) return;
    buffer += text;
    let index;
    while ((index = buffer.indexOf('\n')) >= 0) {
      const segment = buffer.slice(0, index);
      buffer = buffer.slice(index + 1);
      const trimmed = segment.trim();
      if (trimmed) enqueueLine(trimmed);
    }
  };

  const handleClose = () => {
    closed = true;
    flushBuffer();
    if (resolver) {
      const pending = resolver;
      resolver = null;
      pending.reject(new Error('WebSocket closed'));
    }
  };

  function open() {
    if (socket) {
      if (socket.readyState === WebSocket.OPEN) return Promise.resolve();
      if (socket.readyState === WebSocket.CONNECTING) return new Promise((resolve, reject) => {
        const cleanup = () => {
          socket.removeEventListener('open', onOpen);
          socket.removeEventListener('error', onError);
        };
        const onOpen = () => {
          cleanup();
          resolve();
        };
        const onError = (event) => {
          cleanup();
          reject(new Error('WebSocket error'));
        };
        socket.addEventListener('open', onOpen);
        socket.addEventListener('error', onError);
      });
    }
    if (typeof WebSocket === 'undefined') return Promise.reject(new Error('WebSocket unsupported'));
    closed = false;
    buffer = '';
    queue = [];
    socket = new WebSocket(url);
    socket.binaryType = 'arraybuffer';
    return new Promise((resolve, reject) => {
      const onOpen = () => {
        socket.addEventListener('message', handleMessage);
        socket.addEventListener('close', handleClose);
        resolve();
      };
      const onError = (event) => {
        handleClose();
        reject(new Error('WebSocket error'));
      };
      socket.addEventListener('open', onOpen, { once: true });
      socket.addEventListener('error', onError, { once: true });
    });
  }

  function writeLine(line) {
    if (!socket || socket.readyState !== WebSocket.OPEN) {
      return Promise.reject(new Error('WebSocket not connected'));
    }
    socket.send(`${typeof line === 'string' ? line.trim() : String(line)}\n`);
    return Promise.resolve();
  }

  function nextLine() {
    if (queue.length) {
      return Promise.resolve(queue.shift());
    }
    if (closed) {
      return Promise.reject(new Error('WebSocket closed'));
    }
    return new Promise((resolve, reject) => {
      resolver = { resolve, reject };
    });
  }

  function close() {
    if (!socket) return Promise.resolve();
    if (socket.readyState === WebSocket.CLOSING || socket.readyState === WebSocket.CLOSED) {
      return Promise.resolve();
    }
    socket.close();
    return Promise.resolve();
  }

  return {
    open,
    writeLine,
    nextLine,
    close,
    rawPort: { getInfo: () => null }
  };
}

export function createRuntime({
  schemaUrl = './config_schema.json',
  localManifest,
  migrations = {},
  useSimulator = false,
  rpcTimeoutMs = RPC_TIMEOUT_MS,
  testHooks,
  wsUrl
} = {}) {
  const { emit, on } = makeEmitter();

  let transport = null;
  let readTimer = null;
  let readLoopActive = false;
  let queuedTelemetry = null;
  let remoteManifest = null;
  let schema = null;
  let validator = null;
  let liveConfig = null;
  let stagedConfig = null;
  let dirty = false;
  let seq = 0;
  let lastKnownChecksum = null;
  let potGuard = new Set();
  const statusListeners = new Set();
  let rpcSeq = 0;
  const rpcQueue = [];
  let rpcBusy = false;
  let lastRpcTimestamp = 0;
  const pendingRpc = new Map();
  let macroPending = null;
  let macroAvailability = true;
  let scenePending = null;

  const ajv = new Ajv({ strict: false, allErrors: true });
  addFormats(ajv);

  const hooks =
    testHooks ?? (typeof globalThis !== 'undefined' ? globalThis.__MN42_TEST_HOOKS : null) ?? null;
  const rpcTimeout = Number.isFinite(Number(rpcTimeoutMs)) ? Number(rpcTimeoutMs) : RPC_TIMEOUT_MS;
  const params =
    typeof window !== 'undefined' && typeof URLSearchParams === 'function' && typeof window.location === 'object'
      ? new URLSearchParams(window.location.search)
      : null;
  let websocketUrl =
    typeof wsUrl === 'string' && wsUrl.trim().length
      ? wsUrl.trim()
      : params?.get('ws')?.trim() ?? null;

  function getPortInfo(port) {
    if (!port?.getInfo) return null;
    try {
      const info = port.getInfo();
      if (!info) return null;
      return {
        usbVendorId: info.usbVendorId,
        usbProductId: info.usbProductId
      };
    } catch (_) {
      return null;
    }
  }

  function persistPortInfo(port) {
    const info = getPortInfo(port);
    if (!info) return;
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(info));
    } catch (err) {
      console.debug('persist port info failed', err);
    }
  }

  function readPortPreference() {
    try {
      const raw = localStorage.getItem(STORAGE_KEY);
      if (!raw) return null;
      return JSON.parse(raw);
    } catch (err) {
      console.debug('read port preference failed', err);
      return null;
    }
  }

  async function requestPort() {
    if (useSimulator) return createSimulator();
    if (!navigator.serial?.requestPort) throw new Error('WebSerial unavailable');
    const remembered = readPortPreference();
    const filters = remembered ? [remembered] : undefined;
    const port = await navigator.serial.requestPort(filters ? { filters } : {});
    persistPortInfo(port);
    return createTransportPort(port);
  }

  function notifyStatus(payload) {
    if (!payload || !statusListeners.size) return;
    const snapshot = clone(payload);
    for (const listener of [...statusListeners]) {
      try {
        listener(snapshot);
      } catch (err) {
        console.error('runtime status listener error', err);
      }
    }
  }

  function onStatus(handler) {
    if (typeof handler !== 'function') return () => {};
    statusListeners.add(handler);
    return () => statusListeners.delete(handler);
  }

  function createCompletionLatch() {
    // Serial transport stays strictly ordered, so each queued RPC waits until its response
    // releases this latch before the next write goes out.
    let released = false;
    let resolveLatch = () => {};
    const promise = new Promise((resolve) => {
      resolveLatch = resolve;
    });
    return {
      promise,
      release() {
        if (released) return;
        released = true;
        resolveLatch();
      }
    };
  }

  async function processRpcQueue() {
    if (rpcBusy || !transport || !rpcQueue.length) return;
    rpcBusy = true;
    try {
      while (transport && rpcQueue.length) {
        // Keep one in-flight RPC at a time: firmware responses are line-oriented and keyed by id,
        // so serialized writes avoid cross-talk when links get jittery.
        const message = rpcQueue[0];
        const entry = pendingRpc.get(message.id);
        if (!entry) {
          rpcQueue.shift();
          continue;
        }
        const elapsed = performance.now() - lastRpcTimestamp;
        const wait = Math.max(0, RPC_THROTTLE_INTERVAL_MS - elapsed);
        if (wait) {
          await new Promise((resolve) => setTimeout(resolve, wait));
        }
        try {
          await transport.writeLine(JSON.stringify(message));
        } catch (err) {
          if (entry.timer) {
            clearTimeout(entry.timer);
            entry.timer = null;
          }
          pendingRpc.delete(message.id);
          entry.release?.();
          entry.reject(err);
          flushRpcPending(err);
          break;
        }
        lastRpcTimestamp = performance.now();
        entry.timer = setTimeout(() => {
          const pending = pendingRpc.get(message.id);
          if (!pending) return;
          if (pending.timer) {
            clearTimeout(pending.timer);
            pending.timer = null;
          }
          pending.release?.();
          pendingRpc.delete(message.id);
          pending.reject(new Error('RPC timeout'));
        }, entry.timeoutMs);
        await entry.completion;
        rpcQueue.shift();
      }
    } finally {
      rpcBusy = false;
    }
  }

  function handleRpcResponse(msg) {
    const pending = pendingRpc.get(msg.id);
    if (!pending) return;
    if (pending.timer) {
      clearTimeout(pending.timer);
      pending.timer = null;
    }
    pendingRpc.delete(msg.id);
    if (msg.error) {
      const error = new Error(msg.error.message ?? 'RPC error');
      if (msg.error.code !== undefined) error.code = msg.error.code;
      pending.reject(error);
      pending.release?.();
      return;
    }
    const response = Object.prototype.hasOwnProperty.call(msg, 'result') ? msg.result : msg;
    pending.resolve(response);
    pending.release?.();
  }

  function sendRpc(payload, { timeoutMs } = {}) {
    if (!payload || typeof payload !== 'object' || !payload.rpc) {
      return Promise.reject(new Error('RPC payload must include rpc property'));
    }
    if (!transport) return Promise.reject(new Error('Not connected'));
    const id = ++rpcSeq;
    const message = { ...payload, id };
    const request = new Promise((resolve, reject) => {
      const completion = createCompletionLatch();
      const entry = {
        resolve,
        reject,
        timeoutMs: Number.isFinite(Number(timeoutMs)) ? Number(timeoutMs) : rpcTimeout,
        timer: null,
        completion: completion.promise,
        release: completion.release
      };
      pendingRpc.set(id, entry);
      rpcQueue.push(message);
      processRpcQueue();
    });
    // Any failed RPC rolls staged edits back to the last known-good live config so the UI does
    // not stay in a half-applied state.
    return request.catch(async (err) => {
      try {
        await rollback();
      } catch (rollbackErr) {
        console.debug('rollback failed', rollbackErr);
      }
      throw err;
    });
  }

  function sendMacroCommand(command, { timeoutMs } = {}) {
    if (!command || !MACRO_RESPONSE_KEYS[command]) {
      return Promise.reject(new Error('Unknown macro command'));
    }
    if (!transport) return Promise.reject(new Error('Not connected'));
    if (macroPending) return Promise.reject(new Error('Macro command already in progress'));
    let resolveFn;
    let rejectFn;
    const promise = new Promise((resolve, reject) => {
      resolveFn = resolve;
      rejectFn = reject;
    });
    const pending = { command, resolve: resolveFn, reject: rejectFn, timer: null };
    macroPending = pending;
    const timeout = Number.isFinite(Number(timeoutMs)) ? Number(timeoutMs) : MACRO_COMMAND_TIMEOUT_MS;
    pending.timer = setTimeout(() => {
      settleMacroPending(pending, { error: new Error('Macro command timed out') });
    }, timeout);
    transport.writeLine(command).catch((err) => {
      settleMacroPending(pending, { error: err });
    });
    return promise;
  }

  function settleScenePending(pending, { error, response } = {}) {
    if (!pending) return;
    if (pending.timer) {
      clearTimeout(pending.timer);
      pending.timer = null;
    }
    if (scenePending === pending) {
      scenePending = null;
    }
    if (error) {
      pending.reject(error);
      return;
    }
    pending.resolve(response);
  }

  function sendSceneCommand(payload, { timeoutMs } = {}) {
    if (!payload || typeof payload !== 'object' || typeof payload.cmd !== 'string') {
      return Promise.reject(new Error('Scene command requires cmd property'));
    }
    if (!transport) return Promise.reject(new Error('Not connected'));
    if (scenePending) return Promise.reject(new Error('Scene command already in progress'));
    const expectedKey = SCENE_RESPONSE_KEYS[payload.cmd];
    if (!expectedKey) return Promise.reject(new Error('Unknown scene command'));
    let resolveFn;
    let rejectFn;
    const promise = new Promise((resolve, reject) => {
      resolveFn = resolve;
      rejectFn = reject;
    });
    const pending = {
      command: payload.cmd,
      expectedKey,
      resolve: resolveFn,
      reject: rejectFn,
      timer: null
    };
    scenePending = pending;
    const timeout = Number.isFinite(Number(timeoutMs)) ? Number(timeoutMs) : SCENE_COMMAND_TIMEOUT_MS;
    pending.timer = setTimeout(() => {
      settleScenePending(pending, { error: new Error('Scene command timed out') });
    }, timeout);
    transport.writeLine(JSON.stringify(payload)).catch((err) => {
      settleScenePending(pending, { error: err });
    });
    return promise;
  }

  function handleSceneLine(msg) {
    if (!msg || typeof msg !== 'object') return false;
    const hasSceneField =
      Object.prototype.hasOwnProperty.call(msg, 'scene_saved') ||
      Object.prototype.hasOwnProperty.call(msg, 'scene_recalled') ||
      Array.isArray(msg.scenes);
    if (!hasSceneField) return false;

    if (Array.isArray(msg.scenes)) {
      const scenes = msg.scenes.map((entry) => ({
        slot: Number.isFinite(Number(entry?.slot)) ? Number(entry.slot) : 0,
        name: typeof entry?.name === 'string' ? entry.name : '',
        available: Boolean(entry?.available)
      }));
      emit('scene', { type: 'list', scenes });
    }

    const pending = scenePending;
    if (pending) {
      const expectedKey = pending.expectedKey;
      if (Object.prototype.hasOwnProperty.call(msg, expectedKey)) {
        const success =
          expectedKey === 'scenes' ? true : Boolean(msg[expectedKey]);
        if (success) {
          settleScenePending(pending, { response: msg });
        } else {
          const errorMessage =
            msg.scene_error?.message ?? msg.scene_error ?? `${pending.command} failed`;
          settleScenePending(pending, { error: new Error(errorMessage) });
        }
      }
    }

    if (Object.prototype.hasOwnProperty.call(msg, 'scene_saved')) {
      emit('scene', {
        type: 'saved',
        slot: Number.isFinite(Number(msg.scene_slot)) ? Number(msg.scene_slot) : -1,
        name: typeof msg.scene_name === 'string' ? msg.scene_name : '',
        available: Boolean(msg.scene_available),
        raw: msg
      });
    }
    if (Object.prototype.hasOwnProperty.call(msg, 'scene_recalled')) {
      emit('scene', {
        type: 'recalled',
        slot: Number.isFinite(Number(msg.scene_slot)) ? Number(msg.scene_slot) : -1,
        name: typeof msg.scene_name === 'string' ? msg.scene_name : '',
        available: Boolean(msg.scene_available),
        raw: msg
      });
    }

    return true;
  }

  function requestScenes(options = {}) {
    return sendSceneCommand({ cmd: 'GET_SCENES' }, options);
  }

  function flushRpcPending(error) {
    rpcQueue.length = 0;
    for (const [id, pending] of pendingRpc) {
      if (pending.timer) clearTimeout(pending.timer);
      pending.release?.();
      pending.reject(error);
      pendingRpc.delete(id);
    }
    settleMacroPending(macroPending, { error: error ?? new Error('Connection lost') });
    settleScenePending(scenePending, { error: error ?? new Error('Connection lost') });
  }

  async function connect(existingPort) {
    try {
      emit('status', { stage: 'handshake', level: 'info', message: 'Negotiating manifest…' });
      let candidate = existingPort ?? null;
      if (!candidate) {
        candidate = websocketUrl ? createWebSocketTransport(websocketUrl) : await requestPort();
      }
      transport = candidate;
      hooks?.mutateTransport?.(transport);
      await transport.open();
      lastRpcTimestamp = 0;
      persistPortInfo(transport.rawPort);
      startReadLoop();
      emit('transport-open', transport);
      await performHandshake();
      await hydrate();
      emit('connected', {
        manifest: remoteManifest,
        schema,
        config: liveConfig
      });
    } catch (err) {
      emit('error', err);
      await transport?.close().catch(() => {});
      transport = null;
      flushRpcPending(err ?? new Error('Connection failed'));
      throw err;
    }
  }

  async function performHandshake() {
    try {
      await sendRpc({ rpc: 'hello' });
    } catch (err) {
      console.debug('hello RPC failed', err);
    }
    let manifestPayload;
    try {
      manifestPayload = await sendRpc({ rpc: 'get_manifest' });
    } catch (err) {
      console.debug('get_manifest RPC failed', err);
      manifestPayload = null;
    }
    const manifestData = manifestPayload?.manifest ?? manifestPayload;
    if (manifestData && typeof manifestData === 'object') {
      remoteManifest = manifestData;
    } else {
      remoteManifest = {
        fw_version: 'unknown',
        git_sha: 'offline',
        build_time: new Date().toISOString(),
        schema_version: localManifest?.schema_version,
        slot_count: localManifest?.slot_count,
        pot_count: localManifest?.pot_count,
        envelope_count: localManifest?.envelope_count,
        arg_method_count: ARG_METHOD_NAMES.length,
        led_count: localManifest?.led_count ?? 0,
        free_ram: 0,
        free_flash: 0
      };
    }
    emit('manifest', remoteManifest);
    if (!remoteManifest.schema_version && remoteManifest.schemaVersion) {
      remoteManifest.schema_version = remoteManifest.schemaVersion;
    }
    if (localManifest && remoteManifest.schema_version !== localManifest.schema_version) {
      const key = `${remoteManifest.schema_version}->${localManifest.schema_version}`;
      emit('migration-required', {
        from: remoteManifest.schema_version,
        to: localManifest.schema_version,
        canAdapt: typeof migrations[key] === 'function'
      });
    }
  }

  async function hydrate() {
    schema = await fetch(schemaUrl).then((res) => res.json());
    validator = ajv.compile(schema);
    const configPayload = await sendRpc({ rpc: 'get_config' });
    const config = configPayload?.config ?? configPayload;
    const normalized = normalizeConfig(config, remoteManifest ?? localManifest ?? {});
    liveConfig = clone(normalized);
    stagedConfig = clone(normalized);
    dirty = false;
    emit('schema', schema);
    broadcastConfig({ persist: false });
    const snapshot = readStateSnapshot();
    if (snapshot?.staged) {
      stage(() => snapshot.staged);
    }
    startReadLoop();
  }

  function startReadLoop() {
    if (readLoopActive) return;
    readLoopActive = true;
    const pump = async () => {
      readTimer = null;
      if (!transport) {
        readLoopActive = false;
        return;
      }
      try {
        const line = await transport.nextLine();
        handleLine(line);
      } catch (err) {
        emit('error', err);
        await disconnect();
        return;
      } finally {
        if (transport) {
          readTimer = setTimeout(pump, TELEMETRY_FRAME_MS);
        } else {
          readLoopActive = false;
        }
      }
    };
    pump();
  }

  function settleMacroPending(pending, { error, response } = {}) {
    if (!pending) return;
    if (pending.timer) {
      clearTimeout(pending.timer);
      pending.timer = null;
    }
    if (macroPending === pending) {
      macroPending = null;
    }
    if (error) {
      pending.reject(error);
    } else {
      pending.resolve(response);
    }
  }

  function handleMacroLine(msg) {
    if (!msg || typeof msg !== 'object') return false;
    const hasMacroField =
      Object.prototype.hasOwnProperty.call(msg, 'macro_saved') ||
      Object.prototype.hasOwnProperty.call(msg, 'macro_recalled') ||
      Object.prototype.hasOwnProperty.call(msg, 'macro_available');
    if (!hasMacroField) return false;
    if (Object.prototype.hasOwnProperty.call(msg, 'macro_available')) {
      macroAvailability = Boolean(msg.macro_available);
    }
    emit('macro', {
      saved: Boolean(msg.macro_saved),
      recalled: Boolean(msg.macro_recalled),
      available: macroAvailability,
      raw: msg
    });
    const pending = macroPending;
    if (pending) {
      const expectedKey = MACRO_RESPONSE_KEYS[pending.command];
      if (expectedKey && Object.prototype.hasOwnProperty.call(msg, expectedKey)) {
        const success = Boolean(msg[expectedKey]);
        if (success) {
          settleMacroPending(pending, { response: msg });
        } else {
          const errorMessage = msg.error?.message ?? `${pending.command} failed`;
          settleMacroPending(pending, { error: new Error(errorMessage) });
        }
      }
    }
    return true;
  }

  function handleLine(line) {
    // Dispatch order matters: scene/macro replies can look like normal JSON payloads, so route
    // their handlers first before generic RPC/telemetry parsing.
    if (!line) return;
    let msg;
    try {
      msg = JSON.parse(line);
    } catch (err) {
      emit('log', line);
      return;
    }
    if (handleSceneLine(msg)) return;
    if (handleMacroLine(msg)) return;
    if (msg?.id !== undefined) {
      handleRpcResponse(msg);
      return;
    }
    if (msg.type === 'telemetry' || msg.slots || msg.envelopes) {
      queuedTelemetry = { ...(queuedTelemetry || {}), ...msg };
      if (typeof requestAnimationFrame === 'function') {
        requestAnimationFrame(flushTelemetry);
      } else {
        flushTelemetry();
      }
      return;
    }
    if (msg.type === 'config-patch') {
      applyConfigPatch(msg);
      return;
    }
    if (msg.type === 'slot_patch' && msg.slot && typeof msg.slot === 'object') {
      const slotBody = { ...msg.slot };
      const index = extractSlotIndex(slotBody, msg.slot_index ?? msg.index ?? msg.id);
      if (index !== null) {
        slotBody.index = index;
        applyConfigPatch({ slots: [slotBody] });
      }
      return;
    }
    if (msg.type === 'error') {
      emit('device-error', msg);
      return;
    }
    if (msg.status || msg.event || msg.type === 'status') {
      notifyStatus(msg);
      return;
    }
    emit('log', line);
  }

  function flushTelemetry() {
    if (!queuedTelemetry) return;
    const frame = clone(queuedTelemetry);
    emit('telemetry', frame);
    notifyStatus({ type: 'telemetry', ...frame });
    queuedTelemetry = null;
  }

  function persistStateSnapshot() {
    if (typeof localStorage === 'undefined') return;
    try {
      if (stagedConfig === null || stagedConfig === undefined) {
        localStorage.removeItem(STATE_STORAGE_KEY);
        return;
      }
      localStorage.setItem(
        STATE_STORAGE_KEY,
        JSON.stringify({
          schema_version: remoteManifest?.schema_version ?? localManifest?.schema_version,
          staged: stagedConfig,
          timestamp: Date.now()
        })
      );
    } catch (err) {
      console.debug('persist state snapshot failed', err);
    }
  }

  function readStateSnapshot() {
    if (typeof localStorage === 'undefined') return null;
    try {
      const raw = localStorage.getItem(STATE_STORAGE_KEY);
      if (!raw) return null;
      return JSON.parse(raw);
    } catch (err) {
      console.debug('read state snapshot failed', err);
      return null;
    }
  }

  function broadcastConfig({ persist = true } = {}) {
    const payload = { config: clone(liveConfig), staged: clone(stagedConfig), dirty };
    if (persist) persistStateSnapshot();
    console.debug('[runtime] broadcastConfig dirty=', dirty);
    emit('config', payload);
  }

  function restoreLocalState() {
    const snapshot = readStateSnapshot();
    if (!snapshot?.staged) return false;
    stage(() => snapshot.staged);
    return true;
  }

  function replaceConfig(configPayload) {
    if (!configPayload || typeof configPayload !== 'object') return false;
    const normalized = normalizeConfig(configPayload, remoteManifest ?? localManifest ?? {});
    liveConfig = clone(normalized);
    stagedConfig = clone(normalized);
    dirty = false;
    broadcastConfig();
    return true;
  }

  function applyConfigPatch(patch) {
    // Device-side patches update the live snapshot first, then selectively merge into staged
    // values only where the user has not diverged locally.
    if (!patch || typeof patch !== 'object' || !liveConfig) return;
    const prevLive = clone(liveConfig);
    const nextLive = clone(liveConfig);
    let mutated = false;

    if (Array.isArray(patch.slots)) {
      const slotsSource = Array.isArray(nextLive.slots) ? [...nextLive.slots] : [];
      let slotsChanged = false;
      patch.slots.forEach((entry) => {
        const normalized = normalizeSlotPatchEntry(entry);
        if (!normalized) return;
        const { index, fields } = normalized;
        const current = { ...(slotsSource[index] ?? {}) };
        let updated = false;
        Object.entries(fields).forEach(([key, value]) => {
          if (value === undefined) return;
          if (value && typeof value === 'object' && !Array.isArray(value)) {
            const existing = current[key] && typeof current[key] === 'object' ? current[key] : {};
            const merged = { ...existing, ...value };
            if (!shallowEqual(existing, merged)) {
              current[key] = merged;
              updated = true;
            }
            return;
          }
          if (current[key] !== value) {
            current[key] = value;
            updated = true;
          }
        });
        if (updated) {
          slotsSource[index] = current;
          slotsChanged = true;
        }
      });
      if (slotsChanged) {
        nextLive.slots = slotsSource;
        mutated = true;
      }
    }

    if (Array.isArray(patch.efSlots)) {
      let efSlots = Array.isArray(nextLive.efSlots) ? nextLive.efSlots : [];
      let efChanged = false;
      patch.efSlots.forEach((entry, idx) => {
        const index = coerceIndex(entry?.index ?? idx);
        const slotValue = entry?.slot ?? entry?.value ?? entry?.target;
        if (index === null || slotValue === undefined) return;
        const nextArray = applyEfSlotPatch(efSlots, { index, slot: slotValue });
        if (nextArray !== efSlots) {
          efSlots = nextArray;
          efChanged = true;
        }
      });
      if (efChanged) {
        nextLive.efSlots = efSlots;
        mutated = true;
      }
    }

    if (patch.filter && typeof patch.filter === 'object') {
      const prevFilter = prevLive.filter ?? {};
      const nextFilter = { ...(nextLive.filter ?? {}), ...patch.filter };
      if (JSON.stringify(nextFilter) !== JSON.stringify(nextLive.filter ?? {})) {
        nextLive.filter = nextFilter;
        mutated = true;
        patch.__filterMeta = { prev: prevFilter, next: nextFilter }; // stash for staged reconciliation
      }
    }

    if (patch.arg && typeof patch.arg === 'object') {
      const prevArg = prevLive.arg ?? {};
      const nextArg = { ...(nextLive.arg ?? {}), ...patch.arg };
      if (JSON.stringify(nextArg) !== JSON.stringify(nextLive.arg ?? {})) {
        nextLive.arg = nextArg;
        mutated = true;
        patch.__argMeta = { prev: prevArg, next: nextArg };
      }
    }

    if (!mutated) return;

    let nextStaged;
    if (!stagedConfig || !dirty) {
      nextStaged = clone(nextLive);
    } else {
      nextStaged = clone(stagedConfig);
      if (Array.isArray(patch.slots) && Array.isArray(nextLive.slots)) {
        const prevSlots = Array.isArray(prevLive.slots) ? prevLive.slots : [];
        const stagedSlots = Array.isArray(nextStaged.slots) ? nextStaged.slots : [];
        patch.slots.forEach((entry) => {
          const normalized = normalizeSlotPatchEntry(entry);
          if (!normalized) return;
          const { index, fields } = normalized;
          const prevSlot = prevSlots[index] ?? {};
          const stagedSlot = stagedSlots[index] ?? {};
          const nextSlot = nextLive.slots[index] ?? {};
          const merged = { ...stagedSlot };
          let stagedChanged = false;
          Object.keys(fields).forEach((key) => {
            if (merged[key] === undefined || merged[key] === prevSlot[key]) {
              if (merged[key] !== nextSlot[key]) {
                merged[key] = nextSlot[key];
                stagedChanged = true;
              }
            }
          });
          if (!stagedSlots[index] && Object.keys(nextSlot).length) {
            stagedSlots[index] = clone(nextSlot);
          } else if (stagedChanged) {
            stagedSlots[index] = merged;
          }
        });
        nextStaged.slots = stagedSlots;
      }

      if (Array.isArray(patch.efSlots) && Array.isArray(nextLive.efSlots)) {
        const prevEf = Array.isArray(prevLive.efSlots) ? prevLive.efSlots : [];
        const stagedEf = Array.isArray(nextStaged.efSlots) ? nextStaged.efSlots : [];
        patch.efSlots.forEach((entry, idx) => {
          const index = coerceIndex(entry?.index ?? idx);
          if (index === null) return;
          const nextVal = nextLive.efSlots[index]?.slot;
          const prevVal = prevEf[index]?.slot;
          const stagedVal = stagedEf[index]?.slot;
          if (nextVal === undefined) return;
          if (stagedVal === undefined || stagedVal === prevVal) {
            stagedEf[index] = { slot: nextVal };
          }
        });
        nextStaged.efSlots = stagedEf;
      }

      if (patch.__filterMeta) {
        const { prev, next } = patch.__filterMeta;
        nextStaged.filter = nextStaged.filter ?? {};
        Object.keys(next).forEach((key) => {
          if (nextStaged.filter[key] === undefined || nextStaged.filter[key] === prev[key]) {
            nextStaged.filter[key] = next[key];
          }
        });
      }

      if (patch.__argMeta) {
        const { prev, next } = patch.__argMeta;
        nextStaged.arg = nextStaged.arg ?? {};
        Object.keys(next).forEach((key) => {
          if (nextStaged.arg[key] === undefined || nextStaged.arg[key] === prev[key]) {
            nextStaged.arg[key] = next[key];
          }
        });
      }
    }

    if (patch.__filterMeta) delete patch.__filterMeta;
    if (patch.__argMeta) delete patch.__argMeta;

    const normalizedLive = normalizeConfig(nextLive, remoteManifest ?? localManifest ?? {});
    const normalizedStaged = normalizeConfig(nextStaged, remoteManifest ?? localManifest ?? {});
    liveConfig = clone(normalizedLive);
    stagedConfig = clone(normalizedStaged);
    broadcastConfig();
  }

  function stage(updater) {
    const next = typeof updater === 'function' ? updater(clone(stagedConfig)) : updater;
    if (!next) return;
    const normalizedLive = normalizeConfig(liveConfig, remoteManifest ?? localManifest ?? {});
    const normalizedStaged = normalizeConfig(next, remoteManifest ?? localManifest ?? {});
    liveConfig = clone(normalizedLive);
    stagedConfig = clone(normalizedStaged);
    dirty = shallowDiff(normalizedLive ?? {}, normalizedStaged ?? {}).length > 0;
    broadcastConfig();
  }

  function applyPatch(path, value) {
    if (!path || typeof path !== 'string') {
      return Promise.reject(new Error('Invalid patch path'));
    }
    // Stage the payload locally so the UI stays in sync while we hit the device.
    stage((draft) => {
      setNestedValue(draft, path, value);
      return draft;
    });
    // Apply the patch with the JSON-RPC kernel so we can reuse the same timeout/throttle.
    return sendRpc({ rpc: 'set_param', path, value });
  }

  function getState() {
    return {
      manifest: remoteManifest,
      schema,
      live: clone(liveConfig),
      staged: clone(stagedConfig),
      dirty,
      lastChecksum: lastKnownChecksum
    };
  }

  async function apply() {
    if (!dirty) return { applied: false };
    if (!validator(stagedConfig)) {
      const error = new Error('Schema validation failed');
      error.validation = validator.errors;
      emit('validation-error', validator.errors);
      throw error;
    }
    const payload = {
      rpc: 'set_config',
      seq: ++seq,
      schema_version: schema?.schema_version || schema?.properties?.schema_version?.default,
      manifest: {
        fw_version: remoteManifest?.fw_version,
        git_sha: remoteManifest?.git_sha,
        build_time: remoteManifest?.build_time,
        schema_version: remoteManifest?.schema_version
      },
      config: clone(stagedConfig)
    };
    const body = JSON.stringify(payload);
    const checksum = await digest(body);
    payload.checksum = checksum;
    const response = await sendRpc(payload);
    const ackChecksum = response?.checksum ?? response?.result?.checksum ?? null;
    if (ackChecksum !== checksum) {
      await rollback();
      throw new Error('Device failed to acknowledge apply');
    }
    liveConfig = clone(stagedConfig);
    dirty = false;
    lastKnownChecksum = checksum;
    broadcastConfig();
    emit('applied', { checksum });
    return { applied: true, checksum };
  }

  async function rollback() {
    stagedConfig = clone(liveConfig);
    dirty = false;
    broadcastConfig();
    emit('rollback', {});
  }

  async function disconnect() {
    if (!transport) return;
    try {
      await transport.close();
    } finally {
      transport = null;
      if (readTimer) {
        clearTimeout(readTimer);
        readTimer = null;
      }
      readLoopActive = false;
      flushRpcPending(new Error('Disconnected'));
      emit('disconnected');
    }
  }

  function diff() {
    return shallowDiff(liveConfig ?? {}, stagedConfig ?? {});
  }

  function setPotGuard(indices, enabled) {
    indices.forEach((idx) => {
      if (enabled) potGuard.add(idx);
      else potGuard.delete(idx);
    });
    emit('pot-guard', { guards: new Set(potGuard) });
  }

  return {
    connect,
    disconnect,
    stage,
    apply,
    rollback,
    diff,
    getState,
    on,
    onStatus,
    sendRpc,
    sendMacroCommand,
    sendSceneCommand,
    requestScenes,
    applyPatch,
    restoreLocalState,
    replaceConfig,
    setPotGuard,
    createThrottle,
    requestPort,
    useSimulator(toggle) {
      useSimulator = toggle;
    }
  };
}

export { createSimulator };
