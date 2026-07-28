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
import {
  chunkString,
  clone,
  createThrottle,
  decoder,
  digest,
  encoder,
  makeEmitter,
  setNestedValue,
  shallowDiff,
  shallowEqual
} from './runtime/runtime_utils.js';

// Browser-side firmware translator. It keeps three stories in tune: live device
// state, staged editor state, and the simulator state learners can abuse safely.
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

// Runtime shell. Views call this, then mostly listen for events instead of
// poking transport guts directly.
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
  let contractQuality = useSimulator ? 'simulator' : 'incompatible';
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
  const { structuredBridgePreference, resolvedBridgeApiBaseUrl, websocketUrl, bridgeEventsUrl, bridgeControlToken } =
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

  function schemaMigrationRequired() {
    if (useSimulator) return false;
    const deviceVersion = remoteManifest?.schema_version;
    const appVersion = localManifest?.schema_version;
    return (
      deviceVersion !== null && deviceVersion !== undefined &&
      appVersion !== null && appVersion !== undefined &&
      String(deviceVersion) !== String(appVersion)
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
    getDeviceIdentity: () => ({
      device_name: remoteManifest?.device_name ?? null,
      firmware_git_sha: remoteManifest?.git_sha ?? null,
      slot_count: remoteManifest?.slot_count ?? null
    }),
    now: () => Date.now()
  });
  if (typeof document !== 'undefined') {
    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState === 'hidden') stateSnapshotStore.flushPersist();
    });
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
            scenes: true,
            arp_live: true,
            clock_live: true,
            usb_midi_toggle: true,
            note_dynamics_live: true,
            jitter_live: true,
            device_schema: true,
            bulk_config: true,
            one_shot_config_boot: false
          }
        }),
      argMethodNames: ARG_METHOD_NAMES,
      efFilterNames: EF_FILTER_NAMES,
      cloneValue: clone,
      setNested: setNestedValue,
      telemetryFrameMs: TELEMETRY_FRAME_MS
    });
  }

  async function requestPort({ ignorePreference = false } = {}) {
    if (useSimulator) return createSimulatorTransport();
    if (!navigator.serial?.requestPort) throw new Error('WebSerial unavailable');
    const remembered = ignorePreference ? null : portPreferenceStore.read();
    const filters = remembered ? [remembered] : undefined;
    let port;
    try {
      port = await navigator.serial.requestPort(filters ? { filters } : {});
    } catch (err) {
      if (!filters) throw err;
      // A remembered VID/PID may be stale after a boot-mode or firmware identity change.
      portPreferenceStore.clear();
      port = await navigator.serial.requestPort();
    }
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

  function sendRpc(payload, { timeoutMs, rollbackPolicy = 'none' } = {}) {
    if (!['none', 'staged-config'].includes(rollbackPolicy)) {
      throw new Error(`Unsupported RPC rollback policy: ${rollbackPolicy}`);
    }
    const request = rpcKernel.sendRpc(payload, { timeoutMs });
    if (rollbackPolicy === 'none') return request;
    // Staged-config rollback is deliberately opt-in. Profile, live-control, and read RPCs
    // must not discard unrelated configuration edits when they fail.
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
      const response = await client.applyConfig({
        expectedSessionRevision: bridgeSessionRuntime?.getSessionRevision()
      });
      if (response?.session) bridgeSessionRuntime?.applyAuthoritativeSession(response.session);
      const result = response?.result ?? { applied: false, reason: 'missing-bridge-result' };
      return {
        ...result,
        authoritativeConfig: response?.session?.liveConfig ?? null
      };
    },
    refreshBridgeSession: async () =>
      bridgeSessionRuntime?.refreshSessionSnapshot({
        warm: false,
        emitConnectedConfig: false
      }),
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
        eventUrl,
        controlToken: bridgeControlToken
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
      telemetryRuntime.reset();
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
          contractQuality = schemaMigrationRequired()
            ? 'migration-required'
            : schemaSource === 'device'
              ? 'verified'
              : 'fallback-schema';
          emit('contract-quality', {
            quality: contractQuality,
            applyAllowed: contractQuality === 'verified'
          });
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
          bridgeSessionRuntime.reset({ preserveLocalDraft: true });
          emit('status', {
            stage: 'bridge-session',
            level: 'warn',
            message: `Structured bridge session unavailable, falling back to raw bridge transport: ${
              err.message || String(err)
            }`
          });
        }
      }
      const handshake = await performConnectionHandshake({
        sendRpc,
        emit,
        localManifest,
        localSlotMetaManager,
        migrations,
        argMethodCount: ARG_METHOD_NAMES.length
      });
      remoteManifest = handshake.manifest;
      emit('manifest', remoteManifest);
      await hydrate({ handshakeQuality: handshake.quality });
      emit('connected', {
        manifest: remoteManifest,
        schema,
        config: configSession.mergeLocalSlotMeta(configSession.getLiveConfig())
      });
    } catch (err) {
      telemetryRuntime.reset();
      emit('error', err);
      await transport?.close().catch(() => {});
      transport = null;
      flushRpcPending(err ?? new Error('Connection failed'));
      throw err;
    }
  }

  async function hydrate({ handshakeQuality = 'verified' } = {}) {
    const schemaSelection = await selectSchemaForHydration({
      sendRpc,
      schemaUrl,
      emit
    });
    schema = schemaSelection.schema;
    schemaSource = schemaSelection.source;
    contractQuality = useSimulator
      ? 'simulator'
      : schemaMigrationRequired()
        ? 'migration-required'
        : handshakeQuality !== 'verified'
          ? handshakeQuality
          : schemaSelection.quality;
    if (remoteManifest) remoteManifest.contract_quality = contractQuality;
    emit('contract-quality', { quality: contractQuality, applyAllowed: ['verified', 'simulator'].includes(contractQuality) });
    validator = ajv.compile(schema);
    const supportsChunkedConfig = Boolean(
      !useSimulator && remoteManifest?.capabilities?.chunked_reads?.config
    );
    const configPayload = await sendRpc({
      rpc: supportsChunkedConfig ? 'get_config_chunked' : 'get_config'
    });
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
    reconcileDevicePatch: configSession.reconcileDevicePatch,
    clone,
    normalizeConfig,
    shallowEqual,
    getManifest: () => remoteManifest ?? localManifest ?? {},
    broadcastConfig: configSession.broadcastConfig,
    onConflict: (conflicts) => emit('config-conflict', { conflicts })
  });

  const applyPatch = (...args) => liveControlsRuntime.applyPatch(...args);

  async function disconnect() {
    bridgeSessionRuntime.reset({ preserveLocalDraft: true });
    bridgeSessionActive = false;
    telemetryRuntime.reset();
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
    if (!['verified', 'simulator'].includes(contractQuality)) {
      throw new Error(`Apply is blocked until the device contract is verified (current: ${contractQuality}).`);
    }
    if (bridgeSessionActive) {
      if (!bridgeSessionRuntime.isHealthy()) {
        throw new Error('Apply is blocked while the Bridge session event authority is stale.');
      }
      await bridgeSessionRuntime.flushStageSync({ active: bridgeSessionActive });
    }
    return configSession.apply();
  }

  async function rollback() {
    bridgeSessionRuntime.cancelStageSync();
    return configSession.rollback();
  }

  async function resynchronize() {
    return configSession.resynchronize();
  }

  function getState() {
    return {
      ...configSession.getState(),
      transportMode: getTransportMode({ useSimulator, bridgeSessionActive, websocketUrl }),
      contractQuality,
      bridgeSessionActive,
      bridgeSessionHealth: bridgeSessionRuntime.getHealth(),
      telemetryHealth: telemetryRuntime.getHealth(),
      bridgeApiBaseUrl: resolvedBridgeApiBaseUrl
    };
  }

  return {
    connect,
    disconnect,
    stage,
    apply,
    rollback,
    resynchronize,
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
    discardSavedWorkspace: stateSnapshotStore.clear,
    replaceConfig: configSession.replaceConfig,
    setPotGuard,
    setLocalSlotMeta: configSession.setLocalSlotMeta,
    createThrottle,
    requestPort,
    forgetRememberedPort: portPreferenceStore.clear,
    useSimulator(toggle) {
      useSimulator = toggle;
    }
  };
}

export { createSimulator };
