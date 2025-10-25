import Ajv from 'https://cdn.jsdelivr.net/npm/ajv@8/dist/ajv7.mjs';
import addFormats from 'https://cdn.jsdelivr.net/npm/ajv-formats@3/dist/ajv-formats.mjs';

const DEFAULT_DEBOUNCE = 24;
const TELEMETRY_FRAME_MS = 16;
const ACK_TIMEOUT_MS = 1500;
const STORAGE_KEY = 'moarknobs:last-port';
const EF_FILTER_NAMES = [
  'LINEAR',
  'OPPOSITE_LINEAR',
  'EXPONENTIAL',
  'RANDOM',
  'LOWPASS',
  'HIGHPASS',
  'BANDPASS'
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

function chunkBuffer(buffer, chunkSize) {
  if (buffer.length <= chunkSize) return [buffer];
  const out = [];
  for (let i = 0; i < buffer.length; i += chunkSize) {
    out.push(buffer.slice(i, i + chunkSize));
  }
  return out;
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
  arg.method = clamp(methodIndex, 0, ARG_METHOD_NAMES.length - 1);
  arg.method_name = ARG_METHOD_NAMES[arg.method] || defaults.method_name;
  const followerMax = Math.max(0, efLimit - 1);
  arg.sourceA = clamp(Number.isFinite(Number(arg.sourceA)) ? Number(arg.sourceA) : defaults.sourceA, 0, followerMax || 0);
  arg.sourceB = clamp(Number.isFinite(Number(arg.sourceB)) ? Number(arg.sourceB) : defaults.sourceB, 0, followerMax || 0);
  return arg;
}

function normalizeSlotConfig(slot, efLimit = 6) {
  const next = slot && typeof slot === 'object' ? { ...slot } : {};
  next.ef = normalizeSlotEnvelope(next);
  next.efIndex = Number.isFinite(Number(next.efIndex)) ? Number(next.efIndex) : next.ef.index;
  if (!Number.isFinite(Number(next.efIndex))) next.efIndex = next.ef.index;
  next.arg = normalizeSlotArg(next, efLimit);
  return next;
}

function normalizeConfig(config, manifest = {}) {
  if (!config || typeof config !== 'object') return config;
  const next = { ...config };
  const slotCount = manifest.slot_count ?? (Array.isArray(config.slots) ? config.slots.length : 0);
  const efCount = manifest.max_table_lengths?.efSlots ?? (Array.isArray(config.efSlots) ? config.efSlots.length : 0);
  const ledCount = manifest.max_table_lengths?.ledColors ?? (Array.isArray(config.ledColors) ? config.ledColors.length : 0);

  next.slots = Array.from({ length: slotCount }, (_, idx) => normalizeSlotConfig(config.slots?.[idx], efCount));

  next.efSlots = Array.from({ length: efCount }, (_, idx) => {
    const entry = config.efSlots?.[idx];
    const slotIndex = entry && Number.isFinite(Number(entry.slot)) ? Number(entry.slot) : 0;
    const capped = clamp(slotIndex, 0, Math.max(0, slotCount - 1));
    return { slot: capped };
  });

  next.ledColors = Array.from({ length: ledCount }, (_, idx) => {
    const entry = Array.isArray(config.ledColors) ? config.ledColors[idx] : null;
    if (entry && typeof entry.color === 'string') return { color: entry.color };
    return { color: '#000000' };
  });

  if (next.filter && typeof next.filter === 'object') {
    next.filter = { ...next.filter };
  }

  if (next.arg && typeof next.arg === 'object') {
    const globalArg = { ...next.arg };
    if (globalArg.method && typeof globalArg.method !== 'string') {
      const idx = Number(globalArg.method);
      if (Number.isFinite(idx) && ARG_METHOD_NAMES[idx]) {
        globalArg.method = ARG_METHOD_NAMES[idx];
      }
    }
    next.arg = globalArg;
  }

  return next;
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
    fw_git: 'deadbeef',
    build_ts: new Date().toISOString(),
    schema_version: '1.5.0',
    slot_count: 42,
    capabilities: { atomicApply: true },
    max_table_lengths: { ledColors: 42, efSlots: 6 },
    free: { ram: 48000, flash: 512000 }
  };
  const config = {
    slots: Array.from({ length: manifest.slot_count }, (_, idx) => {
      const efIndex = idx % manifest.max_table_lengths.efSlots;
      const filterIndex = idx % EF_FILTER_NAMES.length;
      const argMethod = idx % ARG_METHOD_NAMES.length;
      return {
        id: idx,
        active: idx % 2 === 0,
        type: 'CC',
        midiChannel: (idx % 16) + 1,
        data1: (idx % 120) + 1,
        efIndex,
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
        arg: {
          enabled: idx % 3 === 0,
          method: argMethod,
          method_name: ARG_METHOD_NAMES[argMethod],
          sourceA: efIndex,
          sourceB: (efIndex + 1) % manifest.max_table_lengths.efSlots
        },
        pot: true,
        sysexTemplate: ''
      };
    }),
    efSlots: Array.from({ length: manifest.max_table_lengths.efSlots }, (_, idx) => ({
      slot: (idx * 7) % manifest.slot_count
    })),
    arg: { method: 'PLUS', enable: true, a: 1, b: 1 },
    filter: { type: 'LOWPASS', freq: 800, q: 1 },
    ledColors: Array.from({ length: manifest.max_table_lengths.ledColors }, (_, idx) => ({
      color: `#${((idx * 6151) % 0xffffff).toString(16).padStart(6, '0')}`
    }))
  };
  const telemetry = () => ({
    slots: Array.from({ length: manifest.slot_count }, () => Math.floor(Math.random() * 127)),
    envelopes: Array.from({ length: manifest.max_table_lengths.efSlots }, () => Math.floor(Math.random() * 127)),
    currentSlot: index++ % manifest.slot_count,
    free: manifest.free
  });

  async function open() {
    opened = true;
  }

  async function writeLine(line) {
    if (!opened) throw new Error('simulator closed');
    if (line.startsWith('GET_MANIFEST') || line.startsWith('HELLO')) {
      lines.push(JSON.stringify(manifest));
    } else if (line.startsWith('GET_CONFIG')) {
      lines.push(JSON.stringify(config));
    } else if (line.startsWith('SET_ALL')) {
      try {
        const payload = JSON.parse(line.replace(/^SET_ALL\s+/, ''));
        if (payload?.config) {
          Object.assign(config, payload.config);
        }
        lines.push(JSON.stringify({ type: 'ack', checksum: payload?.checksum || 'sim' }));
      } catch (err) {
        lines.push(JSON.stringify({ type: 'error', message: err.message }));
      }
    } else if (line.startsWith('PING')) {
      lines.push(JSON.stringify({ type: 'pong' }));
    }
    queueMicrotask(() => {
      if (resolver) {
        const fn = resolver;
        resolver = null;
        fn(lines.shift());
      }
    });
  }

  function nextLine() {
    if (!opened) return Promise.reject(new Error('simulator closed'));
    if (lines.length) return Promise.resolve(lines.shift());
    return new Promise((resolve) => {
      resolver = resolve;
      setTimeout(() => {
        if (!resolver) return;
        const fn = resolver;
        resolver = null;
        lines.push(JSON.stringify({ type: 'telemetry', ...telemetry() }));
        fn(lines.shift());
      }, TELEMETRY_FRAME_MS * 4);
    });
  }

  async function close() {
    opened = false;
  }

  return { open, writeLine, nextLine, close, rawPort: { getInfo: () => ({ usbVendorId: 0xfeed, usbProductId: 0xbeef }) } };
}

export function createRuntime({
  schemaUrl = './config_schema.json',
  localManifest,
  migrations = {},
  useSimulator = false
} = {}) {
  const { emit, on } = makeEmitter();

  let transport = null;
  let telemetryTimer = null;
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

  const ajv = new Ajv({ strict: false, allErrors: true });
  addFormats(ajv);

  const outboundQueue = [];
  let writing = false;
  let lastSend = 0;

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

  async function connect(existingPort) {
    try {
      emit('status', { stage: 'handshake', level: 'info', message: 'Negotiating manifest…' });
      transport = existingPort ?? (await requestPort());
      await transport.open();
      persistPortInfo(transport.rawPort);
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
      throw err;
    }
  }

  async function performHandshake() {
    let manifestRaw;
    try {
      manifestRaw = await send('GET_MANIFEST');
    } catch (err) {
      try {
        manifestRaw = await send('HELLO');
      } catch (_) {
        manifestRaw = JSON.stringify({
          fw_version: 'unknown',
          schema_version: localManifest?.schema_version,
          slot_count: localManifest?.slot_count,
          max_table_lengths: localManifest?.max_table_lengths || {},
          capabilities: { atomicApply: true },
          free: { ram: 0, flash: 0 }
        });
      }
    }
    remoteManifest = JSON.parse(manifestRaw);
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
    const raw = await send('GET_CONFIG');
    const config = JSON.parse(raw);
    const normalized = normalizeConfig(config, remoteManifest ?? localManifest ?? {});
    liveConfig = clone(normalized);
    stagedConfig = clone(normalized);
    dirty = false;
    emit('schema', schema);
    emit('config', { config: clone(liveConfig), staged: clone(stagedConfig), dirty });
    scheduleTelemetry();
  }

  function scheduleTelemetry() {
    if (telemetryTimer) return;
    const pump = async () => {
      telemetryTimer = null;
      if (!transport) return;
      try {
        const line = await transport.nextLine();
        handleLine(line);
      } catch (err) {
        emit('error', err);
        await disconnect();
      } finally {
        if (transport) telemetryTimer = setTimeout(pump, TELEMETRY_FRAME_MS);
      }
    };
    telemetryTimer = setTimeout(pump, TELEMETRY_FRAME_MS);
  }

  function handleLine(line) {
    if (!line) return;
    let msg;
    try {
      msg = JSON.parse(line);
    } catch (err) {
      emit('log', line);
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
    if (msg.type === 'ack') {
      emit('ack', msg);
      return;
    }
    if (msg.type === 'error') {
      emit('device-error', msg);
      return;
    }
    emit('log', line);
  }

  function flushTelemetry() {
    if (!queuedTelemetry) return;
    emit('telemetry', queuedTelemetry);
    queuedTelemetry = null;
  }

  function applyConfigPatch(patch) {
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
    dirty = JSON.stringify(stagedConfig) !== JSON.stringify(liveConfig);
    emit('config', { config: clone(liveConfig), staged: clone(stagedConfig), dirty });
  }

  function stage(updater) {
    const next = typeof updater === 'function' ? updater(clone(stagedConfig)) : updater;
    if (!next) return;
    const normalizedLive = normalizeConfig(liveConfig, remoteManifest ?? localManifest ?? {});
    const normalizedStaged = normalizeConfig(next, remoteManifest ?? localManifest ?? {});
    liveConfig = clone(normalizedLive);
    stagedConfig = clone(normalizedStaged);
    dirty = JSON.stringify(stagedConfig) !== JSON.stringify(liveConfig);
    emit('config', { config: clone(liveConfig), staged: clone(stagedConfig), dirty });
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

  async function send(cmd) {
    if (!transport) throw new Error('Not connected');
    return enqueue(cmd, { reply: true });
  }

  async function enqueue(line, { reply = false } = {}) {
    return new Promise((resolve, reject) => {
      outboundQueue.push({ line, reply, resolve, reject });
      pumpQueue();
    });
  }

  async function pumpQueue() {
    if (writing) return;
    writing = true;
    while (outboundQueue.length && transport) {
      const job = outboundQueue.shift();
      try {
        const now = performance.now();
        const wait = Math.max(0, DEFAULT_DEBOUNCE - (now - lastSend));
        if (wait) await new Promise((r) => setTimeout(r, wait));
        await transport.writeLine(job.line);
        lastSend = performance.now();
        if (job.reply) {
          const line = await transport.nextLine();
          job.resolve(line);
        } else {
          job.resolve();
        }
      } catch (err) {
        job.reject(err);
      }
    }
    writing = false;
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
      seq: ++seq,
      schema_version: schema?.schema_version || schema?.properties?.schema_version?.default,
      manifest: {
        fw_version: remoteManifest?.fw_version,
        fw_git: remoteManifest?.fw_git,
        build_ts: remoteManifest?.build_ts,
        schema_version: remoteManifest?.schema_version
      },
      config: clone(stagedConfig)
    };
    const body = JSON.stringify(payload);
    const checksum = await digest(body);
    payload.checksum = checksum;
    const json = JSON.stringify(payload);
    const frames = chunkBuffer(json, 512);
    let index = 0;
    for (const chunk of frames) {
      await enqueue(`SET_ALL ${chunk}`, { reply: false });
      emit('progress', { type: 'chunk', index, total: frames.length });
      index += 1;
    }
    const ack = await waitForAck(checksum);
    if (!ack) {
      await rollback();
      throw new Error('Device failed to acknowledge apply');
    }
    liveConfig = clone(stagedConfig);
    dirty = false;
    lastKnownChecksum = checksum;
    emit('config', { config: clone(liveConfig), staged: clone(stagedConfig), dirty });
    emit('applied', { checksum });
    return { applied: true, checksum };
  }

  async function waitForAck(checksum) {
    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        off();
        resolve(false);
      }, ACK_TIMEOUT_MS);
      const off = on('ack', (msg) => {
        if (!msg) return;
        if (msg.checksum && checksum && msg.checksum !== checksum) {
          return;
        }
        clearTimeout(timeout);
        off();
        resolve(true);
      });
    });
  }

  async function rollback() {
    stagedConfig = clone(liveConfig);
    dirty = false;
    emit('config', { config: clone(liveConfig), staged: clone(stagedConfig), dirty });
    emit('rollback', {});
  }

  async function disconnect() {
    if (!transport) return;
    try {
      await transport.close();
    } finally {
      transport = null;
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
    setPotGuard,
    createThrottle,
    requestPort,
    useSimulator(toggle) {
      useSimulator = toggle;
    }
  };
}

export { createSimulator };
