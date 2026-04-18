import Ajv from './lib/mini-ajv.js';
import addFormats from './lib/add-formats.js';
import { EF_FILTER_NAMES, ARG_METHOD_NAMES } from './lib/constants.js';
import { createLocalManifest } from './manifest_contract.js';
import {
  createTransportPort,
  createSimulator,
  createWebSocketTransport
} from './runtime/transports.js';
import { createRpcKernel } from './runtime/rpc_kernel.js';
import { createPatchReconciler, extractSlotIndex } from './runtime/patch_reconcile.js';
import { normalizeConfig } from './runtime/config_normalize.js';
import { createLocalSlotMetaManager } from './runtime/local_slot_meta.js';
import { createPortPreferenceStore } from './runtime/port_preference.js';
import { createStateSnapshotStore } from './runtime/state_snapshot.js';
import { createRuntimeLineHandler } from './runtime/line_router.js';
import { selectSchemaForHydration } from './runtime/schema_selection.js';
import { performConnectionHandshake } from './runtime/connection_handshake.js';

const DEFAULT_DEBOUNCE = 24;
const TELEMETRY_FRAME_MS = 16;
const RPC_THROTTLE_INTERVAL_MS = 1000 / 120;
const RPC_TIMEOUT_MS = 3000;
const MACRO_COMMAND_TIMEOUT_MS = 6000;
const NATIVE_SET_ALL_CHUNK_SIZE = 96;
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
const LOCAL_SLOT_META_STORAGE_KEY = 'moarknobs:slot-meta';

// Small event bus shared by the runtime and the view layer.
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

// Avoid repeated constructor churn when we encode serial/WebSocket payloads.
function encoder() {
  return new TextEncoder();
}

// Keep a matching decoder helper beside the encoder for transport adapters.
function decoder() {
  return new TextDecoder();
}

// Hash staged config payloads so Apply can demand an explicit firmware ACK.
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

// Structured clone when available; JSON clone fallback keeps tests/browser support simple.
function clone(value) {
  if (value === undefined) return value;
  if (typeof structuredClone === 'function') return structuredClone(value);
  return JSON.parse(JSON.stringify(value));
}

// Diff staged vs live state for the dirty badge and human-readable change panel.
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

// Cheap field-level equality check for tiny metadata records.
function shallowEqual(a, b) {
  if (a === b) return true;
  if (typeof a !== 'object' || typeof b !== 'object' || !a || !b) return false;
  const keys = new Set([...Object.keys(a), ...Object.keys(b)]);
  for (const key of keys) {
    if (a[key] !== b[key]) return false;
  }
  return true;
}

// Convert dotted paths like `slots.3.data1` into array/object access tokens.
function parseSegment(segment) {
  const idx = Number(segment);
  return Number.isInteger(idx) && String(idx) === segment ? idx : segment;
}

// Mutate a nested draft object in place for staged edits and test harness helpers.
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

// Chunk large native `SET_ALL` bodies so serial transport stays within line limits.
function chunkString(value, size) {
  const text = typeof value === 'string' ? value : String(value ?? '');
  const chunkSize = Math.max(1, Math.floor(Number(size) || 1));
  const chunks = [];
  for (let index = 0; index < text.length; index += chunkSize) {
    chunks.push(text.slice(index, index + chunkSize));
  }
  return chunks.length ? chunks : [''];
}

// UI throttling helper for controls that should feel live without flooding transport.
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

// Native serial transport wrapper that exposes the same API as the simulator and WS bridge.
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
  let schemaSource = 'bundled';
  let validator = null;
  let liveConfig = null;
  let stagedConfig = null;
  let dirty = false;
  let seq = 0;
  let lastKnownChecksum = null;
  let potGuard = new Set();
  const statusListeners = new Set();
  let macroPending = null;
  let macroAvailability = true;
  let scenePending = null;

  const ajv = new Ajv({ strict: false, allErrors: true });
  addFormats(ajv);

  const hooks =
    testHooks ?? (typeof globalThis !== 'undefined' ? globalThis.__MN42_TEST_HOOKS : null) ?? null;
  const rpcTimeout = Number.isFinite(Number(rpcTimeoutMs)) ? Number(rpcTimeoutMs) : RPC_TIMEOUT_MS;
  const params =
    typeof window !== 'undefined' &&
    typeof URLSearchParams === 'function' &&
    typeof window.location === 'object'
      ? new URLSearchParams(window.location.search)
      : null;
  let websocketUrl =
    typeof wsUrl === 'string' && wsUrl.trim().length
      ? wsUrl.trim()
      : params?.get('ws')?.trim() ?? null;

  const portPreferenceStore = createPortPreferenceStore({
    storage: typeof localStorage === 'undefined' ? null : localStorage,
    storageKey: STORAGE_KEY
  });

  function currentSlotCount() {
    return (
      liveConfig?.slots?.length ??
      stagedConfig?.slots?.length ??
      remoteManifest?.slot_count ??
      localManifest?.slot_count ??
      0
    );
  }

  const localSlotMetaManager = createLocalSlotMetaManager({
    storageKey: LOCAL_SLOT_META_STORAGE_KEY,
    initialSlotCount: localManifest?.slot_count ?? 0,
    getSlotCount: currentSlotCount,
    cloneValue: clone,
    shallowEqual
  });
  localSlotMetaManager.readFromStorage(localManifest?.slot_count ?? 0);
  const stateSnapshotStore = createStateSnapshotStore({
    storage: typeof localStorage === 'undefined' ? null : localStorage,
    storageKey: STATE_STORAGE_KEY,
    getSchemaVersion: () => remoteManifest?.schema_version ?? localManifest?.schema_version,
    now: () => Date.now()
  });

  function extractLocalSlotMetaFromConfig(config) {
    localSlotMetaManager.extractFromConfig(config);
  }

  function mergeLocalSlotMeta(config) {
    return localSlotMetaManager.mergeIntoConfig(config);
  }

  function createSimulatorTransport() {
    return createSimulator({
      createManifest: () =>
        createLocalManifest({
          uiVersion: 'simulator',
          argMethodCount: ARG_METHOD_NAMES.length,
          capabilities: {
            profile_save: true,
            profile_load: true,
            profile_reset: true,
            macro_snapshot: true,
            scenes: true
          }
        }),
      argMethodNames: ARG_METHOD_NAMES,
      efFilterNames: EF_FILTER_NAMES,
      cloneValue: clone,
      setNested: setNestedValue,
      telemetryFrameMs: TELEMETRY_FRAME_MS
    });
  }

  async function requestPort() {
    if (useSimulator) return createSimulatorTransport();
    if (!navigator.serial?.requestPort) throw new Error('WebSerial unavailable');
    const remembered = portPreferenceStore.read();
    const filters = remembered ? [remembered] : undefined;
    const port = await navigator.serial.requestPort(filters ? { filters } : {});
    portPreferenceStore.persist(port);
    return createTransportPort(port, {}, { makeEncoder: encoder, makeDecoder: decoder });
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

  function isJsonRpcTransport() {
    return (transport?.protocol ?? 'json-rpc') === 'json-rpc';
  }

  function isManifestPayload(msg) {
    return (
      msg &&
      typeof msg === 'object' &&
      !Array.isArray(msg) &&
      typeof msg.device_name === 'string' &&
      Number.isFinite(Number(msg.slot_count)) &&
      Number.isFinite(Number(msg.pot_count))
    );
  }

  function isConfigPayload(msg) {
    return (
      msg &&
      typeof msg === 'object' &&
      !Array.isArray(msg) &&
      Array.isArray(msg.pots) &&
      Array.isArray(msg.slots) &&
      msg.led &&
      typeof msg.led === 'object'
    );
  }

  const rpcKernel = createRpcKernel({
    getTransport: () => transport,
    isJsonRpcTransport,
    chunkString,
    nativeSetAllChunkSize: NATIVE_SET_ALL_CHUNK_SIZE,
    rpcTimeoutMs: rpcTimeout,
    rpcThrottleIntervalMs: RPC_THROTTLE_INTERVAL_MS,
    onFatalError: (error) => {
      settleMacroPending(macroPending, { error });
      settleScenePending(scenePending, { error });
    }
  });

  function sendRpc(payload, { timeoutMs } = {}) {
    const request = rpcKernel.sendRpc(payload, { timeoutMs });
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
    const timeout = Number.isFinite(Number(timeoutMs))
      ? Number(timeoutMs)
      : MACRO_COMMAND_TIMEOUT_MS;
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
    const timeout = Number.isFinite(Number(timeoutMs))
      ? Number(timeoutMs)
      : SCENE_COMMAND_TIMEOUT_MS;
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
        const success = expectedKey === 'scenes' ? true : Boolean(msg[expectedKey]);
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
    const reason = error ?? new Error('Connection lost');
    rpcKernel.flushPending(reason);
    settleMacroPending(macroPending, { error: reason });
    settleScenePending(scenePending, { error: reason });
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
      portPreferenceStore.persist(transport.rawPort);
      startReadLoop();
      emit('transport-open', transport);
      remoteManifest = await performConnectionHandshake({
        sendRpc,
        emit,
        localManifest,
        localSlotMetaManager,
        migrations,
        argMethodCount: ARG_METHOD_NAMES.length
      });
      await hydrate();
      emit('connected', {
        manifest: remoteManifest,
        schema,
        config: mergeLocalSlotMeta(liveConfig)
      });
    } catch (err) {
      emit('error', err);
      await transport?.close().catch(() => {});
      transport = null;
      flushRpcPending(err ?? new Error('Connection failed'));
      throw err;
    }
  }

  async function hydrate() {
    const schemaSelection = await selectSchemaForHydration({
      sendRpc,
      schemaUrl,
      emit
    });
    schema = schemaSelection.schema;
    schemaSource = schemaSelection.source;
    validator = ajv.compile(schema);
    const configPayload = await sendRpc({ rpc: 'get_config' });
    const config = configPayload?.config ?? configPayload;
    const normalized = normalizeConfig(config, remoteManifest ?? localManifest ?? {});
    liveConfig = clone(normalized);
    stagedConfig = clone(normalized);
    dirty = false;
    emit('schema', schema);
    broadcastConfig({ persist: false });
    const snapshot = stateSnapshotStore.read();
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

  function queueTelemetryFrame(msg) {
    queuedTelemetry = { ...(queuedTelemetry || {}), ...msg };
    if (typeof requestAnimationFrame === 'function') {
      requestAnimationFrame(flushTelemetry);
    } else {
      flushTelemetry();
    }
  }

  const handleLine = createRuntimeLineHandler({
    emit,
    notifyStatus,
    rpcKernel,
    handleSceneLine,
    handleMacroLine,
    isManifestPayload,
    isConfigPayload,
    applyConfigPatch: (...args) => applyConfigPatch(...args),
    extractSlotIndex,
    onTelemetry: queueTelemetryFrame
  });

  function flushTelemetry() {
    if (!queuedTelemetry) return;
    const frame = clone(queuedTelemetry);
    emit('telemetry', frame);
    notifyStatus({ type: 'telemetry', ...frame });
    queuedTelemetry = null;
  }

  function broadcastConfig({ persist = true } = {}) {
    const payload = {
      config: mergeLocalSlotMeta(liveConfig),
      staged: mergeLocalSlotMeta(stagedConfig),
      dirty
    };
    if (persist) stateSnapshotStore.persist(stagedConfig);
    console.debug('[runtime] broadcastConfig dirty=', dirty);
    emit('config', payload);
  }

  function restoreLocalState() {
    const snapshot = stateSnapshotStore.read();
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

  const applyConfigPatch = createPatchReconciler({
    getLiveConfig: () => liveConfig,
    getStagedConfig: () => stagedConfig,
    isDirty: () => dirty,
    setLiveConfig: (next) => {
      liveConfig = next;
    },
    setStagedConfig: (next) => {
      stagedConfig = next;
    },
    clone,
    normalizeConfig,
    shallowEqual,
    getManifest: () => remoteManifest ?? localManifest ?? {},
    broadcastConfig
  });

  function stage(updater) {
    const baseConfig =
      stagedConfig ?? liveConfig ?? normalizeConfig({}, remoteManifest ?? localManifest ?? {});
    const next = typeof updater === 'function' ? updater(clone(baseConfig)) : updater;
    if (!next || typeof next !== 'object') return;
    extractLocalSlotMetaFromConfig(next);
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
      schemaSource,
      live: mergeLocalSlotMeta(liveConfig),
      staged: mergeLocalSlotMeta(stagedConfig),
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

  function setLocalSlotMeta(index, patch = {}) {
    if (!localSlotMetaManager.updateEntry(index, patch)) return false;
    broadcastConfig();
    return true;
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
    setLocalSlotMeta,
    createThrottle,
    requestPort,
    useSimulator(toggle) {
      useSimulator = toggle;
    }
  };
}

export { createSimulator };
