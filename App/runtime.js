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
import { createBridgeSessionRuntime } from './runtime/bridge_session_runtime.js';
import { createLiveControlsRuntime } from './runtime/live_controls_runtime.js';
import { createTelemetryRuntime } from './runtime/telemetry_runtime.js';
import {
  getTransportMode,
  resolveTransportModeOptions,
  wantsStructuredBridgeSession
} from './runtime/transport_mode.js';

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
  let readLoopActive = false;
  let closingTransport = false;
  let remoteManifest = null;
  let schema = null;
  let schemaSource = 'bundled';
  let validator = null;
  let seq = 0;
  const statusListeners = new Set();
  let configSession = null;
  let bridgeSessionActive = false;
  let bridgeSessionRuntime = null;
  let liveControlsRuntime = null;

  const ajv = new Ajv({ strict: false, allErrors: true });
  addFormats(ajv);

  const hooks =
    testHooks ?? (typeof globalThis !== 'undefined' ? globalThis.__MN42_TEST_HOOKS : null) ?? null;
  const rpcTimeout = Number.isFinite(Number(rpcTimeoutMs)) ? Number(rpcTimeoutMs) : RPC_TIMEOUT_MS;
  const locationHref =
    typeof window !== 'undefined' && typeof window.location === 'object'
      ? window.location.href
      : undefined;
  const { structuredBridgePreference, resolvedBridgeApiBaseUrl, websocketUrl, bridgeEventsUrl } =
    resolveTransportModeOptions({
      locationHref,
      bridgeApiBaseUrl,
      bridgeTransportMode,
      wsUrl
    });

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

  const telemetryRuntime = createTelemetryRuntime({
    clone,
    emit,
    notifyStatus
  });

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
      liveControlsRuntime?.onFatalError(error);
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
      const client = bridgeSessionRuntime?.ensureClient();
      if (!client) throw new Error('Bridge session unavailable');
      return client.stageConfig(config);
    },
    applyBridgeConfig: async () => {
      const client = bridgeSessionRuntime?.ensureClient();
      if (!client) throw new Error('Bridge session unavailable');
      return client.applyConfig({});
    },
    rollbackBridgeConfig: async (reason) => {
      const client = bridgeSessionRuntime?.ensureClient();
      if (!client) return { rolledBack: false };
      return client.rollbackConfig(reason);
    }
  });

  bridgeSessionRuntime = createBridgeSessionRuntime({
    baseUrl: resolvedBridgeApiBaseUrl,
    eventUrl: bridgeEventsUrl,
    clone,
    emit,
    createClient: ({ baseUrl, eventUrl }) =>
      createBridgeSessionClient({
        baseUrl,
        eventUrl
      }),
    compileSchema(nextSchema) {
      validator = ajv.compile(nextSchema);
    },
    configSession,
    localManifest,
    currentSlotCount,
    localSlotMetaManager,
    getConnectedPayload: () => ({
      manifest: remoteManifest,
      schema,
      config: configSession.mergeLocalSlotMeta(configSession.getLiveConfig())
    }),
    setRemoteManifest(nextManifest) {
      remoteManifest = nextManifest;
    },
    setSchema(nextSchema) {
      schema = nextSchema;
    },
    setSchemaSource(nextSource) {
      schemaSource = nextSource;
    },
    onTelemetry: telemetryRuntime.queueTelemetryFrame
  });

  liveControlsRuntime = createLiveControlsRuntime({
    emit,
    sendRpc,
    getTransport: () => transport,
    configSession,
    setNestedValue,
    async ensureConfigBootTransport() {
      if (transport) return;
      transport = websocketUrl ? createWebSocketTransport(websocketUrl) : await requestPort();
      hooks?.mutateTransport?.(transport);
      await transport.open();
      portPreferenceStore.persist(transport.rawPort);
      startReadLoop();
    },
    disconnect,
    getUseSimulator: () => useSimulator,
    macroResponseKeys: MACRO_RESPONSE_KEYS,
    macroCommandTimeoutMs: MACRO_COMMAND_TIMEOUT_MS,
    sceneResponseKeys: SCENE_RESPONSE_KEYS,
    sceneCommandTimeoutMs: SCENE_COMMAND_TIMEOUT_MS
  });

  function flushRpcPending(error) {
    const reason = error ?? new Error('Connection lost');
    rpcKernel.flushPending(reason);
    liveControlsRuntime?.onFatalError(reason);
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
      if (
        wantsStructuredBridgeSession({
          useSimulator,
          structuredBridgePreference,
          resolvedBridgeApiBaseUrl
        })
      ) {
        try {
          await bridgeSessionRuntime.refreshSessionSnapshot({
            warm: true,
            emitConnectedConfig: false
          });
          await bridgeSessionRuntime.openStructuredEvents();
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
          bridgeSessionRuntime.reset();
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

  const handleLine = createRuntimeLineHandler({
    emit,
    notifyStatus,
    rpcKernel,
    handleSceneLine: liveControlsRuntime.handleSceneLine,
    handleMacroLine: liveControlsRuntime.handleMacroLine,
    isManifestPayload,
    isConfigPayload,
    applyConfigPatch: (...args) => applyConfigPatch(...args),
    extractSlotIndex,
    onTelemetry: telemetryRuntime.queueTelemetryFrame
  });

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

  const applyPatch = (...args) => liveControlsRuntime.applyPatch(...args);

  async function disconnect() {
    bridgeSessionRuntime.reset();
    bridgeSessionActive = false;
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

  const requestConfiguratorBoot = () => liveControlsRuntime.requestConfiguratorBoot();
  const setPotGuard = (...args) => liveControlsRuntime.setPotGuard(...args);

  function stage(updater) {
    configSession.stage(updater);
    bridgeSessionRuntime.scheduleStageSync({ active: bridgeSessionActive });
  }

  async function apply() {
    if (bridgeSessionActive) {
      await bridgeSessionRuntime.flushStageSync({ active: bridgeSessionActive });
    }
    return configSession.apply();
  }

  async function rollback() {
    bridgeSessionRuntime.cancelStageSync();
    return configSession.rollback();
  }

  function getState() {
    return {
      ...configSession.getState(),
      transportMode: getTransportMode({ useSimulator, bridgeSessionActive, websocketUrl }),
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
    sendMacroCommand: liveControlsRuntime.sendMacroCommand,
    sendSceneCommand: liveControlsRuntime.sendSceneCommand,
    requestScenes: liveControlsRuntime.requestScenes,
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
