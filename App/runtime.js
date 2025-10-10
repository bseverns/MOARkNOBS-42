import Ajv from 'https://cdn.jsdelivr.net/npm/ajv@8/dist/ajv7.mjs';
import addFormats from 'https://cdn.jsdelivr.net/npm/ajv-formats@3/dist/ajv-formats.mjs';

const DEFAULT_DEBOUNCE = 24;
const TELEMETRY_FRAME_MS = 16;
const ACK_TIMEOUT_MS = 1500;
const STORAGE_KEY = 'moarknobs:last-port';

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
    schema_version: '1.3.0',
    slot_count: 42,
    capabilities: { atomicApply: true },
    max_table_lengths: { ledColors: 16, efSlots: 6 },
    free: { ram: 48000, flash: 512000 }
  };
  const config = {
    slots: Array.from({ length: manifest.slot_count }, (_, idx) => ({
      id: idx,
      active: idx % 2 === 0,
      type: 'CC',
      midiChannel: (idx % 16) + 1,
      data1: (idx % 120) + 1,
      efIndex: idx % manifest.max_table_lengths.efSlots,
      pot: true
    })),
    arg: { method: 'blend', enable: true, a: 1, b: 1 },
    filter: { type: 0, freq: 800, q: 1 },
    ledColors: Array.from({ length: manifest.max_table_lengths.ledColors }, () => ({ color: '#ff00ff' }))
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
    liveConfig = clone(config);
    stagedConfig = clone(config);
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

  function stage(updater) {
    const next = typeof updater === 'function' ? updater(clone(stagedConfig)) : updater;
    if (!next) return;
    stagedConfig = clone(next);
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
