import Ajv from './lib/mini-ajv.js';
import addFormats from './lib/add-formats.js';
import { EF_FILTER_NAMES, ARG_METHOD_NAMES, SLOT_TYPE_NAMES } from './lib/constants.js';
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
import { createConfigSession } from './runtime/config_session.js';
import { createBridgeSessionClient } from './runtime/bridge_session_client.js';

const DEFAULT_DEBOUNCE = 24;
const TELEMETRY_FRAME_MS = 16;
const RPC_THROTTLE_INTERVAL_MS = 1000 / 120;
const RPC_TIMEOUT_MS = 3000;
const APPLY_RPC_TIMEOUT_MS = 30000;
const MACRO_COMMAND_TIMEOUT_MS = 6000;
const NATIVE_SET_ALL_CHUNK_SIZE = 96;
const NATIVE_SET_ALL_LINE_PACE_MS = 4;
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

function deriveWebSocketUrl(baseUrl, path) {
  if (!baseUrl || !path) return null;
  try {
    const base = new URL(baseUrl, typeof window !== 'undefined' ? window.location.href : undefined);
    const protocol = base.protocol === 'https:' ? 'wss:' : 'ws:';
    return new URL(path, `${protocol}//${base.host}`).toString();
  } catch (_) {
    return null;
  }
}

// Chunk large native `SET_ALL` bodies so serial transport stays within line limits.
function chunkString(value, size) {
  const text = typeof value === 'string' ? value : String(value ?? '');
  const chunkSize = Math.max(1, Math.floor(Number(size) || 1));
  const chunks = [];
  for (let index = 0; index < text.length; ) {
    let end = Math.min(text.length, index + chunkSize);
    if (end < text.length && text[end] === '{') {
      end += 1;
    }
    chunks.push(text.slice(index, end));
    index = end;
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
  wsUrl,
  bridgeApiBaseUrl,
  bridgeTransportMode
} = {}) {
  const { emit, on } = makeEmitter();

  let transport = null;
  let rpcThrottleTimer = null;
  let readLoopActive = false;
  let closingTransport = false;
  let queuedTelemetry = null;
  let telemetryTraceId = null;
  let telemetryTimer = null;
  let remoteManifest = null;
  let schema = null;
  let schemaSource = 'bundled';
  let validator = null;
  let seq = 0;
  let potGuard = new Set();
  const statusListeners = new Set();
  let macroPending = null;
  let macroAvailability = true;
  let scenePending = null;
  let configSession = null;
  let bridgeSessionClient = null;
  let bridgeSessionActive = false;
  let bridgeSessionCache = null;
  let bridgeStageSyncTimer = null;
  let bridgeStageSyncPromise = null;

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
  const locationHref =
    typeof window !== 'undefined' && typeof window.location === 'object'
      ? window.location.href
      : undefined;
  const currentLocation =
    typeof window !== 'undefined' && typeof window.location === 'object'
      ? new URL(window.location.href)
      : null;
  const inferredBridgeBaseUrl =
    currentLocation && currentLocation.pathname.startsWith('/app/') ? currentLocation.origin : null;
  const structuredBridgePreference =
    bridgeTransportMode ??
    params?.get('bridgeTransport')?.trim() ??
    (inferredBridgeBaseUrl ? 'session' : null);
  const resolvedBridgeApiBaseUrl =
    typeof bridgeApiBaseUrl === 'string' && bridgeApiBaseUrl.trim().length
      ? new URL(bridgeApiBaseUrl, locationHref).toString()
      : inferredBridgeBaseUrl;
  let websocketUrl =
    typeof wsUrl === 'string' && wsUrl.trim().length
      ? wsUrl.trim()
      : params?.get('ws')?.trim() ?? deriveWebSocketUrl(resolvedBridgeApiBaseUrl, '/ws') ?? null;
  const bridgeEventsUrl = deriveWebSocketUrl(resolvedBridgeApiBaseUrl, '/ws/events');

  const portPreferenceStore = createPortPreferenceStore({
    storage: typeof localStorage === 'undefined' ? null : localStorage,
    storageKey: STORAGE_KEY
  });

  function currentSlotCount() {
    return (
      configSession?.getLiveConfig()?.slots?.length ??
      configSession?.getStagedConfig()?.slots?.length ??
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
            scenes: true,
            clock_live: true,
            usb_midi_toggle: true,
            note_dynamics_live: true,
            jitter_live: true
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

  function wantsStructuredBridgeSession() {
    return !useSimulator && structuredBridgePreference === 'session' && !!resolvedBridgeApiBaseUrl;
  }

  function getTransportMode() {
    if (useSimulator) return 'simulator';
    if (bridgeSessionActive) return 'bridge-session';
    if (websocketUrl) return 'bridge-raw';
    return 'direct-webserial';
  }

  function ensureBridgeSessionClient() {
    if (!resolvedBridgeApiBaseUrl) return null;
    if (bridgeSessionClient) return bridgeSessionClient;
    bridgeSessionClient = createBridgeSessionClient({
      baseUrl: resolvedBridgeApiBaseUrl,
      eventUrl: bridgeEventsUrl
    });
    return bridgeSessionClient;
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
    nativeSetAllLinePaceMs: NATIVE_SET_ALL_LINE_PACE_MS,
    rpcTimeoutMs: rpcTimeout,
    rpcThrottleIntervalMs: RPC_THROTTLE_INTERVAL_MS,
    onFatalError: (error) => {
      settleMacroPending(macroPending, { error });
      settleScenePending(scenePending, { error });
    }
  });

  function sendRpc(payload, { timeoutMs, rollbackOnError = true } = {}) {
    const request = rpcKernel.sendRpc(payload, { timeoutMs });
    if (!rollbackOnError) {
      return request;
    }
    // Any failed RPC rolls staged edits back to the last known-good live config so the UI does
    // not stay in a half-applied state.
    return request.catch(async (err) => {
      try {
        await configSession?.rollback();
      } catch (rollbackErr) {
        console.debug('rollback failed', rollbackErr);
      }
      throw err;
    });
  }

  configSession = createConfigSession({
    normalizeConfig,
    clone,
    shallowDiff,
    digest,
    emit,
    sendRpc,
    nextSeq: () => ++seq,
    applyRpcTimeoutMs: APPLY_RPC_TIMEOUT_MS,
    slotTypeNames: SLOT_TYPE_NAMES,
    localSlotMetaManager,
    stateSnapshotStore,
    getManifest: () => remoteManifest ?? localManifest ?? {},
    getRemoteManifest: () => remoteManifest,
    getSchema: () => schema,
    getSchemaSource: () => schemaSource,
    getValidator: () => validator,
    isBridgeSessionActive: () => bridgeSessionActive,
    stageBridgeConfig: async (config) => {
      const client = ensureBridgeSessionClient();
      if (!client) throw new Error('Bridge session unavailable');
      return client.stageConfig(config);
    },
    applyBridgeConfig: async () => {
      const client = ensureBridgeSessionClient();
      if (!client) throw new Error('Bridge session unavailable');
      return client.applyConfig({});
    },
    rollbackBridgeConfig: async (reason) => {
      const client = ensureBridgeSessionClient();
      if (!client) return { rolledBack: false };
      return client.rollbackConfig(reason);
    }
  });

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

  function applyBridgeSessionSnapshot(sessionPayload = {}, { emitConnectedConfig = true } = {}) {
    bridgeSessionCache = clone(sessionPayload);

    const manifest = sessionPayload.manifest;
    if (manifest && typeof manifest === 'object') {
      remoteManifest = manifest;
      localSlotMetaManager.ensureCount(
        remoteManifest?.slot_count ?? localManifest?.slot_count ?? currentSlotCount()
      );
      emit('manifest', remoteManifest);
    }

    if (sessionPayload.schema && typeof sessionPayload.schema === 'object') {
      schema = sessionPayload.schema;
      schemaSource = sessionPayload.schemaSource ?? 'bundled';
      validator = ajv.compile(schema);
      emit('schema', schema);
    } else if (sessionPayload.schemaSource) {
      schemaSource = sessionPayload.schemaSource;
    }

    if (sessionPayload.liveConfig || sessionPayload.stagedConfig) {
      configSession.syncFromSession(sessionPayload);
      configSession.broadcastConfig({ persist: false });
    }

    if (emitConnectedConfig) {
      emit('connected', {
        manifest: remoteManifest,
        schema,
        config: configSession.mergeLocalSlotMeta(configSession.getLiveConfig())
      });
    }
  }

  async function refreshBridgeSessionSnapshot({ warm = false, emitConnectedConfig = false } = {}) {
    const client = ensureBridgeSessionClient();
    if (!client) throw new Error('Bridge session unavailable');
    const session = await client.getSession({ warm });
    if (!session || typeof session !== 'object') {
      throw new Error('Bridge session unavailable');
    }
    if (!session.schema || typeof session.schema !== 'object') {
      throw new Error('Bridge session did not provide a schema');
    }
    applyBridgeSessionSnapshot(session, { emitConnectedConfig });
    return session;
  }

  async function flushBridgeStageSync() {
    if (!bridgeSessionActive || !ensureBridgeSessionClient()) return null;
    if (bridgeStageSyncTimer) {
      clearTimeout(bridgeStageSyncTimer);
      bridgeStageSyncTimer = null;
    }
    if (bridgeStageSyncPromise) return bridgeStageSyncPromise;
    const stagedConfig = clone(configSession.getStagedConfig());
    bridgeStageSyncPromise = ensureBridgeSessionClient()
      .stageConfig(stagedConfig)
      .finally(() => {
        bridgeStageSyncPromise = null;
      });
    return bridgeStageSyncPromise;
  }

  function scheduleBridgeStageSync() {
    if (!bridgeSessionActive) return;
    if (bridgeStageSyncTimer) clearTimeout(bridgeStageSyncTimer);
    bridgeStageSyncTimer = setTimeout(() => {
      void flushBridgeStageSync().catch((err) => {
        emit('status', {
          stage: 'bridge-session',
          level: 'warn',
          message: `Bridge stage sync failed: ${err.message || String(err)}`
        });
      });
    }, 120);
  }

  async function openBridgeStructuredEvents() {
    const client = ensureBridgeSessionClient();
    if (!client) throw new Error('Bridge session unavailable');
    await client.openEvents({
      onEvent(message) {
        if (!message || typeof message !== 'object') return;
        const payload = message.payload ?? {};
        switch (message.event) {
          case 'device.ready':
            if (payload.manifest && typeof payload.manifest === 'object') {
              remoteManifest = payload.manifest;
              emit('manifest', remoteManifest);
            }
            if (payload.schemaSource) schemaSource = payload.schemaSource;
            void refreshBridgeSessionSnapshot({ warm: false, emitConnectedConfig: false }).catch(
              () => {}
            );
            break;
          case 'device.config.live':
            bridgeSessionCache = bridgeSessionCache ?? {};
            bridgeSessionCache.liveConfig = payload.config ?? null;
            bridgeSessionCache.lastApplyResult = payload.lastApplyResult ?? null;
            configSession.syncFromSession({
              ...bridgeSessionCache,
              liveConfig: bridgeSessionCache.liveConfig,
              stagedConfig: bridgeSessionCache.stagedConfig,
              dirty: bridgeSessionCache.dirty
            });
            configSession.broadcastConfig({ persist: false });
            break;
          case 'device.config.staged':
            bridgeSessionCache = bridgeSessionCache ?? {};
            bridgeSessionCache.stagedConfig = payload.config ?? null;
            configSession.syncFromSession({
              ...bridgeSessionCache,
              liveConfig: bridgeSessionCache.liveConfig,
              stagedConfig: bridgeSessionCache.stagedConfig,
              dirty: bridgeSessionCache.dirty
            });
            configSession.broadcastConfig({ persist: false });
            break;
          case 'device.config.dirty':
            bridgeSessionCache = bridgeSessionCache ?? {};
            bridgeSessionCache.dirty = Boolean(payload.dirty);
            configSession.syncFromSession({
              ...bridgeSessionCache,
              liveConfig: bridgeSessionCache.liveConfig,
              stagedConfig: bridgeSessionCache.stagedConfig,
              dirty: bridgeSessionCache.dirty
            });
            configSession.broadcastConfig({ persist: false });
            break;
          case 'device.telemetry':
            if (payload.telemetry && typeof payload.telemetry === 'object') {
              queueTelemetryFrame(payload.telemetry);
            }
            break;
          default:
            break;
        }
      },
      onClose() {
        // Raw /ws stays connected for compatibility and live RPC lanes.
      },
      onError(err) {
        emit('status', {
          stage: 'bridge-session',
          level: 'warn',
          message: `Bridge event stream error: ${err?.message || 'event stream failed'}`
        });
      }
    });
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
      if (wantsStructuredBridgeSession()) {
        try {
          await refreshBridgeSessionSnapshot({ warm: true, emitConnectedConfig: false });
          await openBridgeStructuredEvents();
          bridgeSessionActive = true;
          emit('status', {
            stage: 'bridge-session',
            level: 'ok',
            message:
              'Bridge session cache is active. Raw bridge WebSocket remains available for compatibility.'
          });
          emit('connected', {
            manifest: remoteManifest,
            schema,
            config: configSession.mergeLocalSlotMeta(configSession.getLiveConfig())
          });
          return;
        } catch (err) {
          bridgeSessionActive = false;
          bridgeSessionCache = null;
          ensureBridgeSessionClient()?.closeEvents();
          emit('status', {
            stage: 'bridge-session',
            level: 'warn',
            message: `Structured bridge session unavailable, falling back to raw bridge transport: ${
              err.message || String(err)
            }`
          });
        }
      }
      remoteManifest = await performConnectionHandshake({
        sendRpc,
        emit,
        localManifest,
        localSlotMetaManager,
        migrations,
        argMethodCount: ARG_METHOD_NAMES.length
      });
      emit('manifest', remoteManifest);
      await hydrate();
      emit('connected', {
        manifest: remoteManifest,
        schema,
        config: configSession.mergeLocalSlotMeta(configSession.getLiveConfig())
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
    configSession.syncFromDevice(config);
    emit('schema', schema);
    configSession.broadcastConfig({ persist: false });
    if (!bridgeSessionActive) {
      configSession.restoreLocalState();
    }
    startReadLoop();
  }

  function startReadLoop() {
    if (readLoopActive) return;
    readLoopActive = true;
    const pump = async () => {
      while (transport) {
        try {
          const line = await transport.nextLine();
          if (line) handleLine(line);
        } catch (err) {
          if (!closingTransport) {
            emit('error', err);
            await disconnect();
          }
          break;
        }
      }
      readLoopActive = false;
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

  function mergeTelemetryChunk(current, msg) {
    const next = { ...(current || {}), ...msg };

    if (Array.isArray(msg.slotArgs)) {
      const byIndex = new Map();

      for (const arg of current?.slotArgs || []) {
        if (Number.isInteger(arg.index)) byIndex.set(arg.index, arg);
      }

      for (const arg of msg.slotArgs) {
        if (Number.isInteger(arg.index)) byIndex.set(arg.index, arg);
      }

      next.slotArgs = [...byIndex.entries()].sort(([a], [b]) => a - b).map(([, arg]) => arg);
    }

    next.scopes = [...(current?.scopes || []), msg.scope].filter(Boolean);
    return next;
  }

  function queueTelemetryFrame(msg) {
    if (!msg || typeof msg !== 'object') return;

    const traceId = msg.traceId || null;

    if (telemetryTraceId && traceId && traceId !== telemetryTraceId) {
      flushTelemetry();
    }

    if (traceId) {
      telemetryTraceId = traceId;
    }

    queuedTelemetry = mergeTelemetryChunk(queuedTelemetry, msg);

    if (!telemetryTimer) {
      telemetryTimer = setTimeout(flushTelemetry, 50);
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
    if (telemetryTimer) {
      clearTimeout(telemetryTimer);
      telemetryTimer = null;
    }
    telemetryTraceId = null;
    if (!queuedTelemetry) return;

    const frame = clone(queuedTelemetry);
    emit('telemetry', frame);
    notifyStatus({ type: 'telemetry', ...frame });
    queuedTelemetry = null;
  }

  const applyConfigPatch = createPatchReconciler({
    getLiveConfig: configSession.getLiveConfig,
    getStagedConfig: configSession.getStagedConfig,
    isDirty: configSession.isDirty,
    setLiveConfig: configSession.setLiveConfig,
    setStagedConfig: configSession.setStagedConfig,
    clone,
    normalizeConfig,
    shallowEqual,
    getManifest: () => remoteManifest ?? localManifest ?? {},
    broadcastConfig: configSession.broadcastConfig
  });

  function applyPatch(path, value) {
    if (!path || typeof path !== 'string') {
      return Promise.reject(new Error('Invalid patch path'));
    }
    // Stage the payload locally so the UI stays in sync while we hit the device.
    configSession.stage((draft) => {
      setNestedValue(draft, path, value);
      return draft;
    });
    // Apply the patch with the JSON-RPC kernel so we can reuse the same timeout/throttle.
    return sendRpc({ rpc: 'set_param', path, value });
  }

  async function disconnect() {
    if (bridgeStageSyncTimer) {
      clearTimeout(bridgeStageSyncTimer);
      bridgeStageSyncTimer = null;
    }
    ensureBridgeSessionClient()?.closeEvents();
    bridgeSessionActive = false;
    bridgeSessionCache = null;
    if (!transport) {
      emit('disconnected');
      return;
    }
    closingTransport = true;
    try {
      await transport.close();
    } finally {
      transport = null;
      readLoopActive = false;
      closingTransport = false;
      flushRpcPending(new Error('Disconnected'));
      emit('disconnected');
    }
  }

  async function requestConfiguratorBoot() {
    if (useSimulator) {
      emit('status', {
        stage: 'config-boot',
        level: 'warn',
        message: 'Config boot is only available on a physical Web Serial device.'
      });
      return { requested: false, simulator: true };
    }

    const ownsConnection = !transport;
    if (ownsConnection) {
      transport = websocketUrl ? createWebSocketTransport(websocketUrl) : await requestPort();
      hooks?.mutateTransport?.(transport);
      await transport.open();
      portPreferenceStore.persist(transport.rawPort);
      startReadLoop();
    }

    try {
      emit('status', {
        stage: 'config-boot',
        level: 'info',
        message: 'Requesting one-shot configurator boot…'
      });
      await sendRpc({ rpc: 'enter_config_mode' }, { timeoutMs: 1500 });
      emit('status', {
        stage: 'config-boot',
        level: 'ok',
        message: 'Device is rebooting into configurator mode. Reconnect after USB reappears.'
      });
      return { requested: true };
    } finally {
      await disconnect();
    }
  }

  function setPotGuard(indices, enabled) {
    indices.forEach((idx) => {
      if (enabled) potGuard.add(idx);
      else potGuard.delete(idx);
    });
    emit('pot-guard', { guards: new Set(potGuard) });
  }

  function stage(updater) {
    configSession.stage(updater);
    scheduleBridgeStageSync();
  }

  async function apply() {
    if (bridgeSessionActive) {
      await flushBridgeStageSync();
    }
    return configSession.apply();
  }

  async function rollback() {
    if (bridgeStageSyncTimer) {
      clearTimeout(bridgeStageSyncTimer);
      bridgeStageSyncTimer = null;
    }
    return configSession.rollback();
  }

  function getState() {
    return {
      ...configSession.getState(),
      transportMode: getTransportMode(),
      bridgeSessionActive,
      bridgeApiBaseUrl: resolvedBridgeApiBaseUrl
    };
  }

  return {
    connect,
    disconnect,
    stage,
    apply,
    rollback,
    diff: configSession.diff,
    getState,
    on,
    onStatus,
    sendRpc,
    sendMacroCommand,
    sendSceneCommand,
    requestScenes,
    requestConfiguratorBoot,
    applyPatch,
    restoreLocalState: configSession.restoreLocalState,
    replaceConfig: configSession.replaceConfig,
    setPotGuard,
    setLocalSlotMeta: configSession.setLocalSlotMeta,
    createThrottle,
    requestPort,
    useSimulator(toggle) {
      useSimulator = toggle;
    }
  };
}

export { createSimulator };
