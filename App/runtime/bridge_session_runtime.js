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
  let localDraftGeneration = 0;
  let submittedGeneration = 0;
  let acknowledgedGeneration = 0;
  let newestLocalDraft = null;
  let stageSyncSuspended = false;
  let stageRetryRequested = false;
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
    // Bridge snapshots can acknowledge an older draft while the browser has a
    // newer edit queued. Keep that local intent intact until its generation
    // has reached Bridge, while still accepting live-state/readback updates.
    const preserveNewerLocalDraft = localDraftGeneration > acknowledgedGeneration;
    configSession.syncFromSession({
      ...bridgeSessionCache,
      liveConfig: bridgeSessionCache?.liveConfig,
      stagedConfig: preserveNewerLocalDraft
        ? newestLocalDraft
        : bridgeSessionCache?.stagedConfig,
      dirty: preserveNewerLocalDraft ? true : bridgeSessionCache?.dirty
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

    if (sessionPayload.liveConfig || sessionPayload.stagedConfig) syncCachedSession();

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
    if (!active || stageSyncSuspended || !ensureClient()) return null;
    if (bridgeStageSyncTimer) {
      clearTimeout(bridgeStageSyncTimer);
      bridgeStageSyncTimer = null;
    }
    if (bridgeStageSyncPromise) return bridgeStageSyncPromise;
    bridgeStageSyncPromise = (async () => {
      let receipt = null;
      // A new edit may arrive while an HTTP stage request is in flight. Keep
      // submitting snapshots until Bridge has acknowledged the newest local
      // generation, rather than promoting an older staged image on Apply.
      while (acknowledgedGeneration < localDraftGeneration) {
        const generation = localDraftGeneration;
        submittedGeneration = generation;
        const draft = clone(newestLocalDraft ?? configSession.getStagedConfig());
        receipt = await ensureClient().stageConfig(draft, {
          expectedSessionRevision: bridgeSessionCache?.sessionRevision
        });
        if (Number.isFinite(Number(receipt?.sessionRevision))) {
          bridgeSessionCache = { ...(bridgeSessionCache ?? {}), sessionRevision: receipt.sessionRevision };
        }
        acknowledgedGeneration = generation;
        if (acknowledgedGeneration === localDraftGeneration) newestLocalDraft = null;
      }
      return receipt;
    })()
      .catch(async (err) => {
        if (err?.code === 'stale_session_revision') {
          await refreshSessionSnapshot({ warm: false, emitConnectedConfig: false });
        }
        throw err;
      })
      .finally(() => {
        bridgeStageSyncPromise = null;
        if (stageRetryRequested) queueMicrotask(drainStageRetry);
      });
    return bridgeStageSyncPromise;
  }

  function scheduleStageSync({ active = false } = {}) {
    if (!active) return;
    localDraftGeneration += 1;
    newestLocalDraft = clone(configSession.getStagedConfig());
    if (stageSyncSuspended) return;
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

  function drainStageRetry() {
    if (
      !stageRetryRequested ||
      stageSyncSuspended ||
      bridgeStageSyncPromise
    ) return;

    if (acknowledgedGeneration >= localDraftGeneration) {
      stageRetryRequested = false;
      return;
    }

    stageRetryRequested = false;
    void flushStageSync({ active: true })
      .catch((err) => {
        emit('status', {
          stage: 'bridge-session',
          level: 'warn',
          message: `Bridge stage sync failed: ${err.message || String(err)}`
        });
      })
      .finally(() => {
        if (stageRetryRequested) drainStageRetry();
      });
  }

  function requestStageRetry({ active = true } = {}) {
    if (!active || acknowledgedGeneration >= localDraftGeneration) return;
    stageRetryRequested = true;
    drainStageRetry();
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
          case 'device.apply.pending':
            bridgeSessionCache = {
              ...(bridgeSessionCache ?? {}),
              deviceAuthority: 'applying',
              lastApplyResult: { status: 'pending', ...clone(payload.lastApplyResult ?? payload) }
            };
            syncCachedSession();
            break;
          case 'device.apply.uncertain':
            bridgeSessionCache = { ...(bridgeSessionCache ?? {}), deviceAuthority: 'uncertain', lastApplyResult: { status: 'uncertain', ...clone(payload) } };
            syncCachedSession();
            break;
          case 'device.apply.ack':
            bridgeSessionCache = {
              ...(bridgeSessionCache ?? {}),
              deviceAuthority: 'verified',
              lastApplyResult: { status: 'ack', ...clone(payload) }
            };
            syncCachedSession();
            requestStageRetry();
            break;
          case 'device.apply.rollback':
            bridgeSessionCache = {
              ...(bridgeSessionCache ?? {}),
              deviceAuthority: 'verified',
              lastApplyResult: {
                status: 'rollback',
                ...clone(payload.lastApplyResult ?? payload)
              }
            };
            syncCachedSession();
            requestStageRetry();
            break;
          case 'device.apply.resynchronized':
            bridgeSessionCache = { ...(bridgeSessionCache ?? {}), deviceAuthority: 'verified', lastApplyResult: { status: 'resynchronized', ...clone(payload) } };
            syncCachedSession();
            requestStageRetry();
            break;
          case 'device.apply.device_different':
            bridgeSessionCache = { ...(bridgeSessionCache ?? {}), deviceAuthority: 'verified-device-different', lastApplyResult: { status: 'verified_device_different', ...clone(payload) } };
            syncCachedSession();
            requestStageRetry();
            break;
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
            if (
              payload.sessionRevision !== undefined &&
              Number(payload.sessionRevision) <= Number(bridgeSessionCache?.sessionRevision)
            ) break;
            bridgeSessionCache = bridgeSessionCache ?? {};
            bridgeSessionCache.liveConfig = payload.config ?? null;
            bridgeSessionCache.lastApplyResult = payload.lastApplyResult ?? null;
            bridgeSessionCache.deviceAuthority = payload.deviceAuthority ?? bridgeSessionCache.deviceAuthority;
            bridgeSessionCache.draftState = payload.draftState ?? bridgeSessionCache.draftState;
            if (payload.sessionRevision !== undefined) bridgeSessionCache.sessionRevision = payload.sessionRevision;
            syncCachedSession();
            break;
          case 'device.session.snapshot':
            // A revisioned snapshot is the only event that may atomically move
            // live, staged, and dirty state together.
            if (
              bridgeSessionCache?.sessionRevision !== undefined &&
              Number(payload.sessionRevision) < Number(bridgeSessionCache.sessionRevision)
            ) break;
            bridgeSessionCache = { ...(bridgeSessionCache ?? {}), ...clone(payload) };
            syncCachedSession();
            break;
          case 'device.config.staged':
            if (
              payload.sessionRevision !== undefined &&
              Number(payload.sessionRevision) <= Number(bridgeSessionCache?.sessionRevision)
            ) break;
            bridgeSessionCache = bridgeSessionCache ?? {};
            bridgeSessionCache.stagedConfig = payload.config ?? null;
            bridgeSessionCache.deviceAuthority = payload.deviceAuthority ?? bridgeSessionCache.deviceAuthority;
            bridgeSessionCache.draftState = payload.draftState ?? bridgeSessionCache.draftState;
            if (payload.sessionRevision !== undefined) bridgeSessionCache.sessionRevision = payload.sessionRevision;
            syncCachedSession();
            break;
          case 'device.config.dirty':
            if (
              payload.sessionRevision !== undefined &&
              Number(payload.sessionRevision) <= Number(bridgeSessionCache?.sessionRevision)
            ) break;
            bridgeSessionCache = bridgeSessionCache ?? {};
            bridgeSessionCache.dirty = Boolean(payload.dirty);
            bridgeSessionCache.deviceAuthority = payload.deviceAuthority ?? bridgeSessionCache.deviceAuthority;
            bridgeSessionCache.draftState = payload.draftState ?? bridgeSessionCache.draftState;
            if (payload.sessionRevision !== undefined) bridgeSessionCache.sessionRevision = payload.sessionRevision;
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

  function suspendStageSync() {
    stageSyncSuspended = true;
    cancelStageSync();
  }

  function resumeStageSync({ active = false } = {}) {
    stageSyncSuspended = false;
    requestStageRetry({ active });
  }

  function recordStageReceipt(receipt = {}) {
    bridgeSessionCache = {
      ...(bridgeSessionCache ?? {}),
      ...(receipt.sessionRevision === undefined
        ? {}
        : { sessionRevision: receipt.sessionRevision }),
      clientApplyId: receipt.clientApplyId ?? null,
      stagedRevision: receipt.stagedRevision ?? null,
      stagedDigest: receipt.stagedDigest ?? null
    };
  }

  function recordSessionRevision(sessionRevision) {
    if (!Number.isFinite(Number(sessionRevision))) return;
    bridgeSessionCache = {
      ...(bridgeSessionCache ?? {}),
      sessionRevision: Number(sessionRevision)
    };
  }

  function closeEvents() {
    eventsWanted = false;
    if (eventReconnectTimer) clearTimeout(eventReconnectTimer);
    eventReconnectTimer = null;
    setSessionHealth('disconnected');
    ensureClient()?.closeEvents();
  }

  function reset({ preserveLocalDraft = false } = {}) {
    cancelStageSync();
    stageSyncSuspended = false;
    stageRetryRequested = false;
    closeEvents();
    bridgeSessionCache = null;
    if (!preserveLocalDraft) {
      newestLocalDraft = null;
      localDraftGeneration = 0;
      submittedGeneration = 0;
      acknowledgedGeneration = 0;
    }
  }

  return {
    ensureClient,
    recordStageReceipt,
    recordSessionRevision,
    refreshSessionSnapshot,
    applyAuthoritativeSession: (session) => applySessionSnapshot(session, { emitConnectedConfig: false }),
    flushStageSync,
    scheduleStageSync,
    suspendStageSync,
    resumeStageSync,
    openStructuredEvents,
    cancelStageSync,
    closeEvents,
    reset,
    isHealthy: () => sessionHealth === 'healthy',
    getSessionRevision: () => bridgeSessionCache?.sessionRevision ?? null,
    getHealth: () => ({ health: sessionHealth, lastEventAt })
  };
}
