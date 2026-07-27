export function createBridgeSessionRuntime({
  baseUrl,
  eventUrl,
  clone,
  emit,
  createClient,
  compileSchema,
  configSession,
  localManifest,
  currentSlotCount,
  localSlotMetaManager,
  getConnectedPayload,
  setRemoteManifest,
  setSchema,
  setSchemaSource,
  onTelemetry
} = {}) {
  let bridgeSessionClient = null;
  let bridgeSessionCache = null;
  let bridgeStageSyncTimer = null;
  let bridgeStageSyncPromise = null;
  let eventReconnectTimer = null;
  let eventReconnectAttempt = 0;
  let eventsWanted = false;
  let sessionHealth = 'disconnected';
  let lastEventAt = null;

  function setSessionHealth(next, details = {}) {
    sessionHealth = next;
    emit('bridge-session-health', {
      health: next,
      lastEventAt,
      ...details
    });
  }

  function ensureClient() {
    if (!baseUrl) return null;
    if (bridgeSessionClient) return bridgeSessionClient;
    bridgeSessionClient = createClient({
      baseUrl,
      eventUrl
    });
    return bridgeSessionClient;
  }

  function syncCachedSession() {
    configSession.syncFromSession({
      ...bridgeSessionCache,
      liveConfig: bridgeSessionCache?.liveConfig,
      stagedConfig: bridgeSessionCache?.stagedConfig,
      dirty: bridgeSessionCache?.dirty
    });
    configSession.broadcastConfig({ persist: false });
  }

  function applySessionSnapshot(sessionPayload = {}, { emitConnectedConfig = true } = {}) {
    bridgeSessionCache = clone(sessionPayload);

    const manifest =
      sessionPayload.manifest && typeof sessionPayload.manifest === 'object'
        ? {
            ...sessionPayload.manifest,
            ...(sessionPayload.hardwareHealth && typeof sessionPayload.hardwareHealth === 'object'
              ? sessionPayload.hardwareHealth
              : {})
          }
        : null;
    if (manifest && typeof manifest === 'object') {
      setRemoteManifest(manifest);
      localSlotMetaManager.ensureCount(
        manifest?.slot_count ?? localManifest?.slot_count ?? currentSlotCount()
      );
      emit('manifest', manifest);
    }

    if (sessionPayload.schema && typeof sessionPayload.schema === 'object') {
      setSchema(sessionPayload.schema);
      setSchemaSource(sessionPayload.schemaSource ?? 'bundled');
      compileSchema(sessionPayload.schema);
      emit('schema', sessionPayload.schema);
    } else if (sessionPayload.schemaSource) {
      setSchemaSource(sessionPayload.schemaSource);
    }

    if (sessionPayload.liveConfig || sessionPayload.stagedConfig) {
      configSession.syncFromSession(sessionPayload);
      configSession.broadcastConfig({ persist: false });
    }

    if (emitConnectedConfig) {
      emit('connected', getConnectedPayload());
    }
  }

  async function refreshSessionSnapshot({ warm = false, emitConnectedConfig = false } = {}) {
    const client = ensureClient();
    if (!client) throw new Error('Bridge session unavailable');
    const session = await client.getSession({ warm });
    if (!session || typeof session !== 'object') {
      throw new Error('Bridge session unavailable');
    }
    if (!session.schema || typeof session.schema !== 'object') {
      throw new Error('Bridge session did not provide a schema');
    }
    applySessionSnapshot(session, { emitConnectedConfig });
    return session;
  }

  async function flushStageSync({ active = false } = {}) {
    if (!active || !ensureClient()) return null;
    if (bridgeStageSyncTimer) {
      clearTimeout(bridgeStageSyncTimer);
      bridgeStageSyncTimer = null;
    }
    if (bridgeStageSyncPromise) return bridgeStageSyncPromise;
    const stagedConfig = clone(configSession.getStagedConfig());
    bridgeStageSyncPromise = ensureClient()
      .stageConfig(stagedConfig)
      .finally(() => {
        bridgeStageSyncPromise = null;
      });
    return bridgeStageSyncPromise;
  }

  function scheduleStageSync({ active = false } = {}) {
    if (!active) return;
    if (bridgeStageSyncTimer) clearTimeout(bridgeStageSyncTimer);
    bridgeStageSyncTimer = setTimeout(() => {
      void flushStageSync({ active: true }).catch((err) => {
        emit('status', {
          stage: 'bridge-session',
          level: 'warn',
          message: `Bridge stage sync failed: ${err.message || String(err)}`
        });
      });
    }, 120);
  }

  async function openStructuredEvents() {
    const client = ensureClient();
    if (!client) throw new Error('Bridge session unavailable');
    eventsWanted = true;
    await client.openEvents({
      onEvent(message) {
        lastEventAt = Date.now();
        if (sessionHealth !== 'healthy') setSessionHealth('healthy');
        if (!message || typeof message !== 'object') return;
        const payload = message.payload ?? {};
        switch (message.event) {
          case 'device.ready':
            if (payload.manifest && typeof payload.manifest === 'object') {
              const manifest = {
                ...payload.manifest,
                ...(payload.hardwareHealth && typeof payload.hardwareHealth === 'object'
                  ? payload.hardwareHealth
                  : {})
              };
              setRemoteManifest(manifest);
              emit('manifest', manifest);
            }
            if (payload.schemaSource) setSchemaSource(payload.schemaSource);
            void refreshSessionSnapshot({ warm: false, emitConnectedConfig: false }).catch(
              () => {}
            );
            break;
          case 'device.config.live':
            bridgeSessionCache = bridgeSessionCache ?? {};
            bridgeSessionCache.liveConfig = payload.config ?? null;
            bridgeSessionCache.lastApplyResult = payload.lastApplyResult ?? null;
            syncCachedSession();
            break;
          case 'device.config.staged':
            bridgeSessionCache = bridgeSessionCache ?? {};
            bridgeSessionCache.stagedConfig = payload.config ?? null;
            syncCachedSession();
            break;
          case 'device.config.dirty':
            bridgeSessionCache = bridgeSessionCache ?? {};
            bridgeSessionCache.dirty = Boolean(payload.dirty);
            syncCachedSession();
            break;
          case 'device.telemetry':
            if (payload.telemetry && typeof payload.telemetry === 'object') {
              onTelemetry(payload.telemetry);
            }
            break;
          default:
            break;
        }
      },
      onClose() {
        if (!eventsWanted) return;
        setSessionHealth('reconnecting');
        scheduleEventReconnect();
      },
      onError(err) {
        emit('status', {
          stage: 'bridge-session',
          level: 'warn',
          message: `Bridge event stream error: ${err?.message || 'event stream failed'}`
        });
      }
    });
    eventReconnectAttempt = 0;
    lastEventAt = Date.now();
    setSessionHealth('healthy');
  }

  function scheduleEventReconnect() {
    if (!eventsWanted || eventReconnectTimer) return;
    const delayMs = Math.min(15000, 500 * 2 ** Math.min(eventReconnectAttempt, 5));
    eventReconnectAttempt += 1;
    eventReconnectTimer = setTimeout(async () => {
      eventReconnectTimer = null;
      try {
        await openStructuredEvents();
        // Events alone can arrive as individual cache fields; refresh gives one atomic truth.
        await refreshSessionSnapshot({ warm: false, emitConnectedConfig: false });
      } catch (err) {
        setSessionHealth('stale', { reason: err.message || String(err) });
        scheduleEventReconnect();
      }
    }, delayMs);
  }

  function cancelStageSync() {
    if (bridgeStageSyncTimer) {
      clearTimeout(bridgeStageSyncTimer);
      bridgeStageSyncTimer = null;
    }
  }

  function closeEvents() {
    eventsWanted = false;
    if (eventReconnectTimer) clearTimeout(eventReconnectTimer);
    eventReconnectTimer = null;
    setSessionHealth('disconnected');
    ensureClient()?.closeEvents();
  }

  function reset() {
    cancelStageSync();
    closeEvents();
    bridgeSessionCache = null;
  }

  return {
    ensureClient,
    refreshSessionSnapshot,
    flushStageSync,
    scheduleStageSync,
    openStructuredEvents,
    cancelStageSync,
    closeEvents,
    reset,
    isHealthy: () => sessionHealth === 'healthy',
    getHealth: () => ({ health: sessionHealth, lastEventAt })
  };
}
