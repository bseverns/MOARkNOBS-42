function copyDefined(source, keys, clone) {
  if (!source || typeof source !== 'object') return {};
  const out = {};
  for (const key of keys) {
    if (source[key] !== undefined) out[key] = clone(source[key]);
  }
  return out;
}

function equivalentJson(a, b) {
  return JSON.stringify(a ?? null) === JSON.stringify(b ?? null);
}

function slotTypeForDevice(slot, slotTypeNames) {
  if (typeof slot?.type_name === 'string' && slotTypeNames.includes(slot.type_name)) {
    return slot.type_name;
  }
  if (typeof slot?.type === 'string' && slotTypeNames.includes(slot.type)) {
    return slot.type;
  }
  const numericType = Number(slot?.type);
  if (Number.isInteger(numericType) && slotTypeNames[numericType]) {
    return slotTypeNames[numericType];
  }
  return 'OFF';
}

function resolveEfIndexForDevice(slot) {
  const topLevel = Number(slot?.ef_index ?? slot?.efIndex);
  const nested = Number(slot?.ef?.index);
  if (Number.isFinite(topLevel) && Math.round(topLevel) >= 0) {
    return Math.round(topLevel);
  }
  if (Number.isFinite(nested) && Math.round(nested) >= 0) {
    return Math.round(nested);
  }
  if (Number.isFinite(topLevel)) {
    return Math.round(topLevel);
  }
  if (Number.isFinite(nested)) {
    return Math.round(nested);
  }
  return -1;
}

function compactSlotForDevice(slot, previousSlot, { clone, slotTypeNames }) {
  const out = {};
  out.type = slotTypeForDevice(slot, slotTypeNames);
  if (slot?.channel !== undefined) out.channel = clone(slot.channel);
  else if (slot?.midiChannel !== undefined) out.channel = clone(slot.midiChannel);
  if (slot?.data1 !== undefined) out.data1 = clone(slot.data1);
  else if (slot?.cc !== undefined) out.data1 = clone(slot.cc);
  // Always make hidden per-slot state explicit. Firmware otherwise retains a
  // previous arp note, which makes an omitted browser default fail readback.
  out.arpNote = clone(slot?.arpNote ?? (out.type === 'Note' ? out.data1 ?? 0 : 0));
  out.active = Boolean(slot?.active);
  out.ef_index = resolveEfIndexForDevice(slot);
  if (slot?.sysexTemplate !== undefined && slot?.sysexTemplate !== previousSlot?.sysexTemplate) {
    out.sysexTemplate = clone(slot.sysexTemplate);
  }
  if (
    slot?.ef &&
    typeof slot.ef === 'object' &&
    !equivalentJson(slot.ef, previousSlot?.ef)
  ) {
    out.ef = copyDefined(
      slot.ef,
      [
        'index',
        'filter_index',
        'filter_name',
        'filter',
        'frequency',
        'q',
        'oversample',
        'smoothing',
        'baseline',
        'gain',
        'mode',
        'auto_baseline',
        'autoBaseline',
        'auto_gain',
        'autoGain',
        'attack_ms',
        'attackMs',
        'release_ms',
        'releaseMs',
        'rms_ms',
        'rmsWindowMs',
        'baseline_tau_ms',
        'baselineTauMs',
        'gain_tau_ms',
        'gainTauMs',
        'gate_threshold',
        'gateThreshold',
        'gate_hysteresis',
        'gateHysteresis',
        'activity_threshold',
        'activityThreshold',
        'gain_target',
        'gainTarget',
        'destination_mode',
        'destinationMode',
        'destination_mode_name'
      ],
      clone
    );
  }
  if (
    slot?.ef_payload &&
    typeof slot.ef_payload === 'object' &&
    !equivalentJson(slot.ef_payload, previousSlot?.ef_payload)
  ) {
    out.ef_payload = copyDefined(
      slot.ef_payload,
      ['type_index', 'type', 'freq', 'frequency', 'q'],
      clone
    );
  }
  if (slot?.arg && typeof slot.arg === 'object' && !equivalentJson(slot.arg, previousSlot?.arg)) {
    out.arg = copyDefined(
      slot.arg,
      ['enable', 'enabled', 'method', 'method_name', 'a', 'b', 'sourceA', 'sourceB'],
      clone
    );
  }
  if (Array.isArray(slot?.lfo) && !equivalentJson(slot.lfo, previousSlot?.lfo)) {
    out.lfo = slot.lfo.map((lane) =>
      copyDefined(lane, ['enabled', 'mode', 'amount'], clone)
    );
  }
  return out;
}

export function compactConfigForDevice(config, previousConfig, options) {
  if (!config || typeof config !== 'object') return config;
  const { clone } = options;
  const out = {};
  if (Array.isArray(config.slots)) {
    out.slots = config.slots.map((slot, index) =>
      compactSlotForDevice(slot, previousConfig?.slots?.[index], options)
    );
  }
  if (Array.isArray(config.efSlots) && !equivalentJson(config.efSlots, previousConfig?.efSlots)) {
    out.efSlots = clone(config.efSlots);
  }
  if (
    config.filter &&
    typeof config.filter === 'object' &&
    !equivalentJson(config.filter, previousConfig?.filter)
  ) {
    out.filter = clone(config.filter);
  }
  if (
    config.arg &&
    typeof config.arg === 'object' &&
    !equivalentJson(config.arg, previousConfig?.arg)
  ) {
    out.arg = clone(config.arg);
  }
  if (
    config.led &&
    typeof config.led === 'object' &&
    !equivalentJson(config.led, previousConfig?.led)
  ) {
    out.led = clone(config.led);
  }
  if (
    config.envelopeMode !== undefined &&
    !equivalentJson(config.envelopeMode, previousConfig?.envelopeMode)
  ) {
    out.envelopeMode = clone(config.envelopeMode);
  }
  return out;
}

export function createConfigSession({
  normalizeConfig,
  clone,
  shallowDiff,
  digest,
  emit,
  sendRpc,
  nextSeq,
  applyRpcTimeoutMs,
  slotTypeNames,
  localSlotMetaManager,
  stateSnapshotStore,
  getManifest,
  getRemoteManifest,
  getSchema,
  getSchemaSource,
  getValidator,
  isBridgeSessionActive = () => false,
  stageBridgeConfig = null,
  applyBridgeConfig = null,
  refreshBridgeSession = null,
  rollbackBridgeConfig = null
} = {}) {
  let clientApplyRevision = 0;
  let liveConfig = null;
  let stagedConfig = null;
  let dirty = false;
  // Device truth and local intent are deliberately independent. `transactionState`
  // remains a compatibility projection for existing UI/event consumers.
  let deviceAuthority = 'verified';
  let draftState = 'clean';
  let lastKnownChecksum = null;
  let transactionState = 'clean';
  let attemptedApply = null;
  let appliedCandidate = null;
  let nextDraft = null;

  function isDeviceAuthorityUnresolved() {
    return ['preflighting', 'applying', 'uncertain', 'resynchronizing'].includes(deviceAuthority);
  }

  function setDraftState(next) {
    draftState = next;
  }

  function setTransactionState(next, details = {}) {
    if (next === 'dirty' || next === 'clean') {
      setDraftState(next);
      if (next === 'clean') {
        deviceAuthority = 'verified';
      }
    } else if (next === 'verified') {
      deviceAuthority = 'verified';
      setDraftState(dirty ? 'dirty' : 'clean');
    } else if (next === 'verified-device-different') {
      deviceAuthority = next;
      setDraftState(dirty ? 'dirty' : 'clean');
    } else {
      deviceAuthority = next;
    }
    transactionState = next;
    emit('config-transaction', { state: next, deviceAuthority, draftState, ...details });
  }

  function setBridgeAuthority(authority, nextDraftState, details = {}) {
    deviceAuthority = authority;
    setDraftState(nextDraftState);
    // Preserve the legacy single-state field as a projection, while callers
    // which understand the two dimensions can use the explicit values.
    transactionState = authority === 'verified'
      ? (nextDraftState === 'dirty' ? 'dirty' : 'verified')
      : authority;
    emit('config-transaction', {
      state: transactionState,
      deviceAuthority,
      draftState,
      ...details
    });
  }

  function extractLocalSlotMetaFromConfig(config) {
    localSlotMetaManager.extractFromConfig(config);
  }

  function mergeLocalSlotMeta(config) {
    return localSlotMetaManager.mergeIntoConfig(config);
  }

  function broadcastConfig({ persist = true } = {}) {
    const payload = {
      config: mergeLocalSlotMeta(liveConfig),
      staged: mergeLocalSlotMeta(stagedConfig),
      dirty
    };
    if (persist) {
      if (dirty) {
        const persistDraft = stateSnapshotStore.schedulePersist ?? stateSnapshotStore.persist;
        persistDraft.call(stateSnapshotStore, stagedConfig);
      } else {
        stateSnapshotStore.clear?.();
      }
    }
    console.debug('[runtime] broadcastConfig dirty=', dirty);
    emit('config', payload);
  }

  function syncFromDevice(configPayload) {
    const normalized = normalizeConfig(configPayload, getManifest());
    liveConfig = clone(normalized);
    stagedConfig = clone(nextDraft ?? normalized);
    dirty = shallowDiff(liveConfig ?? {}, stagedConfig ?? {}).length > 0;
    setDraftState(dirty ? 'dirty' : 'clean');
    deviceAuthority = 'verified';
    if (!appliedCandidate) setTransactionState(dirty ? 'dirty' : 'verified');
  }

  function syncFromSession(sessionPayload = {}) {
    const normalizedLive = normalizeConfig(sessionPayload.liveConfig ?? {}, getManifest());
    const normalizedStaged = normalizeConfig(
      sessionPayload.stagedConfig ?? sessionPayload.liveConfig ?? {},
      getManifest()
    );
    liveConfig = clone(normalizedLive);
    const retainedLocalIntent = nextDraft ?? appliedCandidate;
    stagedConfig = clone(retainedLocalIntent ?? normalizedStaged);
    if (retainedLocalIntent) {
      dirty = shallowDiff(normalizedLive ?? {}, stagedConfig ?? {}).length > 0;
    } else if (sessionPayload.dirty === undefined) {
      dirty = shallowDiff(normalizedLive ?? {}, normalizedStaged ?? {}).length > 0;
    } else {
      dirty = Boolean(sessionPayload.dirty);
    }
    const explicitDraftState = ['clean', 'dirty'].includes(sessionPayload.draftState)
      ? sessionPayload.draftState
      : null;
    setDraftState(
      retainedLocalIntent
        ? (dirty ? 'dirty' : 'clean')
        : (explicitDraftState ?? (dirty ? 'dirty' : 'clean'))
    );
    const bridgeStatus = sessionPayload.lastApplyResult?.status;
    const bridgeTransactionState = {
      pending: 'applying',
      uncertain: 'uncertain',
      resynchronized: 'verified',
      verified_device_different: 'verified-device-different',
      rollback: 'clean',
      ack: 'verified'
    }[bridgeStatus];
    const explicitAuthority = [
      'verified',
      'preflighting',
      'applying',
      'uncertain',
      'resynchronizing',
      'verified-device-different'
    ].includes(sessionPayload.deviceAuthority)
      ? sessionPayload.deviceAuthority
      : null;
    const bridgeAuthority = explicitAuthority ?? bridgeTransactionState;
    const bridgeAuthorityStates = new Set([
      'applying',
      'uncertain',
      'resynchronizing',
      'verified-device-different'
    ]);
    if (!appliedCandidate || bridgeAuthorityStates.has(bridgeAuthority)) {
      if (explicitAuthority) {
        setBridgeAuthority(bridgeAuthority, draftState, {
          source: 'bridge',
          lastApplyResult: sessionPayload.lastApplyResult ?? null
        });
        return;
      }
      setTransactionState(bridgeAuthority ?? (dirty ? 'dirty' : 'verified'), {
        source: 'bridge',
        lastApplyResult: sessionPayload.lastApplyResult ?? null
      });
    }
  }

  function stage(updater) {
    const baseConfig = stagedConfig ?? liveConfig ?? normalizeConfig({}, getManifest());
    const next = typeof updater === 'function' ? updater(clone(baseConfig)) : updater;
    if (!next || typeof next !== 'object') return;
    extractLocalSlotMetaFromConfig(next);
    const normalizedLive = normalizeConfig(liveConfig, getManifest());
    const normalizedStaged = normalizeConfig(next, getManifest());
    liveConfig = clone(normalizedLive);
    stagedConfig = clone(normalizedStaged);
    dirty = shallowDiff(normalizedLive ?? {}, normalizedStaged ?? {}).length > 0;
    setDraftState(dirty ? 'dirty' : 'clean');
    if (appliedCandidate || isDeviceAuthorityUnresolved()) {
      nextDraft = clone(normalizedStaged);
    } else {
      // A preflight-rejected candidate remains local intent until it is
      // successfully retried. Any later edit supersedes that retained draft.
      if (nextDraft) nextDraft = clone(normalizedStaged);
      setTransactionState(dirty ? 'dirty' : 'clean');
    }
    broadcastConfig();
  }

  function finishApply(
    authoritativeConfig,
    details = {},
    stateOverride = null,
    { preserveCandidate = false } = {}
  ) {
    const normalized = normalizeConfig(authoritativeConfig, getManifest());
    const retainedDraft = nextDraft ?? (preserveCandidate ? appliedCandidate : null);
    liveConfig = clone(normalized);
    stagedConfig = clone(retainedDraft ?? normalized);
    dirty = shallowDiff(liveConfig ?? {}, stagedConfig ?? {}).length > 0;
    setDraftState(dirty ? 'dirty' : 'clean');
    appliedCandidate = null;
    nextDraft = null;
    // An authoritative completion resolves an applying/resynchronizing device
    // even when a separate next draft remains dirty.
    if (!stateOverride) deviceAuthority = 'verified';
    setTransactionState(stateOverride ?? (dirty ? 'dirty' : 'verified'), details);
  }

  function abandonApply({ retainNextDraft = false } = {}) {
    appliedCandidate = null;
    if (!retainNextDraft) nextDraft = null;
    dirty = shallowDiff(liveConfig ?? {}, stagedConfig ?? {}).length > 0;
    setDraftState(dirty ? 'dirty' : 'clean');
    if (!isDeviceAuthorityUnresolved()) {
      setTransactionState(dirty ? 'dirty' : 'clean');
    }
  }

  function finishBridgeNotApplied(response = {}) {
    const retainedDraft = nextDraft ?? appliedCandidate ?? stagedConfig;
    appliedCandidate = null;
    nextDraft = null;
    stagedConfig = clone(retainedDraft);
    dirty = shallowDiff(liveConfig ?? {}, stagedConfig ?? {}).length > 0;
    setDraftState(dirty ? 'dirty' : 'clean');
    deviceAuthority = 'verified';
    setTransactionState(dirty ? 'dirty' : 'verified', {
      source: 'bridge',
      reason: response?.reason ?? 'not-applied'
    });
    broadcastConfig();
  }

  function finishPreflightRejection(error) {
    const rejectedDraft = nextDraft ?? appliedCandidate ?? stagedConfig;
    stagedConfig = clone(rejectedDraft);
    appliedCandidate = null;
    // Keep the rejected candidate independently of Bridge snapshots. It was
    // never transmitted, so it remains an unacknowledged browser draft.
    nextDraft = clone(rejectedDraft);
    dirty = shallowDiff(liveConfig ?? {}, stagedConfig ?? {}).length > 0;
    setDraftState(dirty ? 'dirty' : 'clean');
    deviceAuthority = 'verified';
    setTransactionState(dirty ? 'dirty' : 'verified', {
      source: 'bridge',
      reason: 'preflight-rejected',
      code: error?.code ?? null
    });
    broadcastConfig();
    emit('bridge-apply-preflight-rejected', { error });
  }

  async function resynchronizeAfterUncertain(reason) {
    setTransactionState('resynchronizing', { reason, attemptedApply });
    broadcastConfig({ persist: false });
    try {
      const remoteManifest = getRemoteManifest();
      const response = await sendRpc(
        {
          rpc: remoteManifest?.capabilities?.chunked_reads?.config
            ? 'get_config_chunked'
            : 'get_config'
        },
        { timeoutMs: applyRpcTimeoutMs }
      );
      const deviceConfig = normalizeConfig(response?.config ?? response, getManifest());
      const candidateMatchesDevice =
        !appliedCandidate ||
        JSON.stringify(deviceConfig) ===
          JSON.stringify(normalizeConfig(appliedCandidate, getManifest()));
      finishApply(
        deviceConfig,
        { reason: 'resynchronized', attemptedApply },
        candidateMatchesDevice ? null : 'verified-device-different',
        { preserveCandidate: !candidateMatchesDevice }
      );
      broadcastConfig();
      emit('resynchronized', { attemptedApply });
      return deviceConfig;
    } catch (resyncError) {
      // Keep the transmitted candidate as the unresolved transaction token.
      // Operator edits are stored in nextDraft and cannot unlock another Apply.
      setTransactionState('uncertain', { reason, attemptedApply, resyncError });
      broadcastConfig({ persist: false });
      emit('apply-uncertain', { reason, attemptedApply, error: resyncError });
      return null;
    }
  }

  async function markApplyUncertain(reason, payload, checksum) {
    attemptedApply = {
      seq: payload.seq,
      checksum,
      reason,
      timestamp: Date.now()
    };
    setTransactionState('uncertain', { reason, attemptedApply });
    // Keep the candidate visible until an authoritative device read succeeds.
    // A transmitted Apply must never be represented as a local rollback.
    broadcastConfig({ persist: false });
    emit('apply-uncertain', { reason, attemptedApply });
    return resynchronizeAfterUncertain(reason);
  }

  function classifyReadbackRecovery(recovered, transmittedConfig, details = {}) {
    const recoveredMatchesCandidate =
      JSON.stringify(normalizeConfig(recovered, getManifest())) ===
      JSON.stringify(normalizeConfig(transmittedConfig, getManifest()));
    return recoveredMatchesCandidate
      ? { applied: true, verifiedBy: 'readback', ...details }
      : { applied: false, verifiedDeviceState: true, verifiedBy: 'readback', ...details };
  }

  async function apply() {
    if (appliedCandidate) {
      throw new Error('Apply is already in progress.');
    }
    if (isDeviceAuthorityUnresolved()) {
      throw new Error('Apply is blocked until the previous transmitted configuration is resynchronized.');
    }
    if (!dirty) return { applied: false };
    const validator = getValidator();
    if (!validator(stagedConfig)) {
      const error = new Error('Schema validation failed');
      error.validation = validator.errors;
      emit('validation-error', validator.errors);
      throw error;
    }
    appliedCandidate = clone(stagedConfig);
    nextDraft = null;
    if (isBridgeSessionActive()) {
      clientApplyRevision += 1;
      setTransactionState('preflighting');
      const applyIdentity = {
        clientApplyId:
          globalThis.crypto?.randomUUID?.() ??
          `app-${Date.now()}-${clientApplyRevision}`,
        stagedRevision: clientApplyRevision,
        stagedDigest: await digest(JSON.stringify(appliedCandidate))
      };
      try {
        const response =
          typeof applyBridgeConfig === 'function'
            ? await applyBridgeConfig({
                candidate: clone(appliedCandidate),
                identity: applyIdentity
              })
            : { applied: false };
        const checksum = response?.checksum ?? response?.result?.checksum ?? null;
        const bridgeApplyResult = response?.result ?? response;
        if (
          bridgeApplyResult?.applied !== true &&
          !(
            bridgeApplyResult?.applied === false &&
            bridgeApplyResult?.reason === 'clean'
          )
        ) {
          const error = new Error('Bridge Apply response omitted its transaction result.');
          error.code = 'invalid_bridge_apply_response';
          error.bridgeFailureClass = 'transmission-unknown';
          error.bridgeSession = response?.session ?? null;
          throw error;
        }
        if (bridgeApplyResult.applied === false) {
          const notAppliedResult = bridgeApplyResult;
          finishBridgeNotApplied(notAppliedResult);
          emit('bridge-apply-not-applied', { response });
          return {
            applied: false,
            reason: notAppliedResult?.reason ?? 'bridge-not-applied'
          };
        }
        const authoritativeConfig =
          response?.authoritativeConfig ??
          response?.result?.authoritativeConfig ??
          appliedCandidate;
        finishApply(authoritativeConfig, { source: 'bridge', checksum });
        lastKnownChecksum = checksum;
        broadcastConfig();
        emit('applied', { checksum });
        return { applied: true, checksum };
      } catch (err) {
        // The Bridge may have staged/applied despite a lost HTTP response, or
        // another client may have advanced its revision. Never erase a local
        // draft solely because this request failed; the session refresh path
        // remains the authority for reconciliation.
        emit('bridge-apply-failed', { error: err, staged: clone(stagedConfig) });
        const failureClass = err?.bridgeFailureClass ?? 'transmission-unknown';
        if (failureClass === 'preflight-rejected') {
          finishPreflightRejection(err);
          throw err;
        }
        // HTTP error responses include the Bridge session snapshot. Keep it
        // as a correlated fallback if the follow-up refresh is unavailable.
        let authoritativeSession = err?.bridgeSession ?? null;
        if (typeof refreshBridgeSession === 'function') {
          try {
            authoritativeSession =
              (await refreshBridgeSession()) ?? authoritativeSession;
          } catch (refreshError) {
            emit('bridge-session-refresh-failed', { error: refreshError });
          }
        }
        const bridgeStatus = authoritativeSession?.lastApplyResult?.status;
        const receipt = authoritativeSession?.lastApplyResult;
        const receiptMatchesAttempt =
          receipt?.clientApplyId === applyIdentity.clientApplyId &&
          receipt?.stagedRevision === applyIdentity.stagedRevision &&
          receipt?.stagedDigest === applyIdentity.stagedDigest;
        if (!receiptMatchesAttempt) {
          // A refreshed session may contain a perfectly valid receipt for an
          // older browser attempt. It says nothing about this failed request.
          setTransactionState('uncertain', {
            source: 'bridge',
            reason: 'uncorrelated-bridge-failure',
            applyIdentity,
            lastApplyResult: receipt ?? null
          });
          broadcastConfig({ persist: false });
          throw err;
        }
        syncFromSession(authoritativeSession);
        if (['uncertain', 'unresolved', 'pending'].includes(bridgeStatus)) {
          setTransactionState(
            bridgeStatus === 'pending' ? 'resynchronizing' : 'uncertain',
            {
              source: 'bridge',
              lastApplyResult: authoritativeSession.lastApplyResult
            }
          );
        } else if (
          ['resynchronized', 'ack', 'verified_device_different'].includes(bridgeStatus) &&
          authoritativeSession?.liveConfig
        ) {
          finishApply(
            authoritativeSession.liveConfig,
            {
              source: 'bridge',
              lastApplyResult: authoritativeSession.lastApplyResult
            },
            bridgeStatus === 'verified_device_different'
              ? 'verified-device-different'
              : null,
            { preserveCandidate: bridgeStatus === 'verified_device_different' }
          );
          broadcastConfig();
        } else if (
          failureClass === 'device-rejected-before-commit' &&
          bridgeStatus === 'rollback' &&
          authoritativeSession?.liveConfig
        ) {
          finishApply(
            authoritativeSession.liveConfig,
            {
              source: 'bridge',
              reason: 'device-rejected-before-commit',
              lastApplyResult: authoritativeSession.lastApplyResult
            },
            null,
            { preserveCandidate: true }
          );
          broadcastConfig();
          emit('bridge-apply-device-rejected', {
            error: err,
            lastApplyResult: authoritativeSession.lastApplyResult
          });
        } else {
          // The Bridge supplied no transaction authority. Preserve the local
          // draft, but release the candidate so a later session snapshot can
          // establish the state.
          abandonApply({ retainNextDraft: true });
        }
        throw err;
      }
    }
    const schema = getSchema();
    const remoteManifest = getRemoteManifest();
    const payload = {
      rpc: 'set_config',
      seq: nextSeq(),
      schema_version: schema?.schema_version || schema?.properties?.schema_version?.default,
      manifest: {
        fw_version: remoteManifest?.fw_version,
        git_sha: remoteManifest?.git_sha,
        build_time: remoteManifest?.build_time,
        schema_version: remoteManifest?.schema_version
      },
      config: clone(appliedCandidate),
      deviceConfig: compactConfigForDevice(appliedCandidate, liveConfig, {
        clone,
        slotTypeNames
      })
    };
    const body = JSON.stringify(payload);
    const checksum = await digest(body);
    payload.checksum = checksum;
    let response;
    setTransactionState('applying', { seq: payload.seq, checksum });
    try {
      response = await sendRpc(payload, { timeoutMs: applyRpcTimeoutMs });
    } catch (err) {
      const recovered = await markApplyUncertain('transport-failure', payload, checksum);
      if (recovered) {
        return classifyReadbackRecovery(recovered, payload.config, {
          ackReceived: false,
          checksum
        });
      }
      if (/RPC timeout/i.test(err?.message ?? '')) {
        throw new Error('Timed out waiting for firmware ACK; device state is being resynchronized.');
      }
      throw err;
    }
    const ackChecksum = response?.checksum ?? response?.result?.checksum ?? null;
    const appliedChecksum = response?.applied_checksum ?? response?.result?.applied_checksum ?? null;
    const storageGeneration = response?.storage_generation ?? response?.result?.storage_generation ?? null;
    if (ackChecksum !== checksum) {
      const recovered = await markApplyUncertain('malformed-ack', payload, checksum);
      if (recovered) {
        return classifyReadbackRecovery(recovered, payload.config, {
          receiptValid: false,
          checksum
        });
      }
      throw new Error('Device failed to acknowledge apply');
    }
    // Hardware bulk Apply is durable only when firmware confirms its own
    // applied-state digest and committed storage generation. Simulators and
    // older manifests retain their compatibility path.
    const requiresHardwareIntegrity = remoteManifest?.persistence?.backend === 'littlefs';
    if (requiresHardwareIntegrity && (!appliedChecksum || storageGeneration === null)) {
      const recovered = await markApplyUncertain('missing-integrity-receipt', payload, checksum);
      if (recovered) {
        return classifyReadbackRecovery(recovered, payload.config, {
          receiptValid: false,
          checksum
        });
      }
      throw new Error('Device ACK omitted applied-state integrity receipt');
    }
    if (requiresHardwareIntegrity) {
      let readback;
      try {
        const readbackResponse = await sendRpc(
          {
            rpc: remoteManifest?.capabilities?.chunked_reads?.config
              ? 'get_config_chunked'
              : 'get_config'
          },
          { timeoutMs: applyRpcTimeoutMs }
        );
        readback = normalizeConfig(readbackResponse?.config ?? readbackResponse, getManifest());
      } catch (err) {
        await markApplyUncertain('readback-failure', payload, checksum);
        throw new Error(`Device applied configuration but readback verification failed: ${err?.message ?? err}`);
      }
      const expected = normalizeConfig(appliedCandidate, getManifest());
      if (JSON.stringify(readback) !== JSON.stringify(expected)) {
        // The device is the authority after an ACK. Preserve its truth instead
        // of promoting an unverified browser-side candidate. Any newer local
        // draft remains staged separately for the operator's next Apply.
        finishApply(
          readback,
          {
            reason: 'readback-mismatch',
            attemptedApply: { seq: payload.seq, checksum }
          },
          'verified-device-different',
          { preserveCandidate: true }
        );
        broadcastConfig();
        throw new Error('Device readback differs from the applied configuration');
      }
      finishApply(readback, { seq: payload.seq, checksum });
    } else {
      finishApply(appliedCandidate, { seq: payload.seq, checksum });
    }
    lastKnownChecksum = checksum;
    attemptedApply = null;
    broadcastConfig();
    emit('applied', { checksum, appliedChecksum, storageGeneration });
    return { applied: true, checksum, appliedChecksum, storageGeneration };
  }

  async function rollback() {
    if (isDeviceAuthorityUnresolved()) {
      throw new Error('Cannot discard a transmitted configuration while device state is uncertain. Resynchronize first.');
    }
    if (isBridgeSessionActive() && typeof rollbackBridgeConfig === 'function') {
      await rollbackBridgeConfig('operator_request');
    }
    stagedConfig = clone(liveConfig);
    dirty = false;
    deviceAuthority = 'verified';
    setTransactionState('clean');
    broadcastConfig();
    emit('rollback', {});
  }

  function restoreLocalState({ allowDifferentFirmware = false } = {}) {
    const canReadSnapshot = typeof stateSnapshotStore.read === 'function';
    const snapshot = stateSnapshotStore.read?.();
    if (canReadSnapshot && !snapshot) return false;
    const identity = stateSnapshotStore.identityDecision?.(snapshot);
    if (['different-firmware', 'unknown-identity'].includes(identity) && !allowDifferentFirmware) {
      emit('snapshot-restore-required', { snapshot, identity });
      return false;
    }
    const staged =
      typeof stateSnapshotStore.readStagedConfig === 'function'
        ? stateSnapshotStore.readStagedConfig({ allowDifferentFirmware })
        : stateSnapshotStore.read()?.staged;
    if (!staged) return false;
    stage(() => staged);
    return true;
  }

  // Accept only configuration returned by an explicitly authoritative device
  // readback/recall path. Browser imports and presets must continue to stage.
  function hydrateAuthoritativeConfig(configPayload) {
    if (!configPayload || typeof configPayload !== 'object') return false;
    const normalized = normalizeConfig(configPayload, getManifest());
    liveConfig = clone(normalized);
    stagedConfig = clone(normalized);
    dirty = false;
    setTransactionState('verified');
    broadcastConfig();
    return true;
  }

  function diff() {
    return shallowDiff(liveConfig ?? {}, stagedConfig ?? {});
  }

  function getState() {
    return {
      manifest: getRemoteManifest(),
      schema: getSchema(),
      schemaSource: getSchemaSource(),
      live: mergeLocalSlotMeta(liveConfig),
      staged: mergeLocalSlotMeta(stagedConfig),
      dirty,
      lastChecksum: lastKnownChecksum,
      transactionState,
      deviceAuthority,
      draftState,
      attemptedApply: attemptedApply ? clone(attemptedApply) : null
    };
  }

  function setLocalSlotMeta(index, patch = {}) {
    if (!localSlotMetaManager.updateEntry(index, patch)) return false;
    broadcastConfig();
    return true;
  }

  function reconcileDevicePatch(nextLive, nextStaged) {
    const unresolvedAuthority = isDeviceAuthorityUnresolved()
      ? deviceAuthority
      : null;
    liveConfig = clone(nextLive);
    stagedConfig = clone(nextStaged);
    dirty = shallowDiff(liveConfig ?? {}, stagedConfig ?? {}).length > 0;
    setDraftState(dirty ? 'dirty' : 'clean');
    if (unresolvedAuthority) {
      setBridgeAuthority(unresolvedAuthority, draftState, {
        source: 'device-patch'
      });
      return;
    }
    deviceAuthority = 'verified';
    setTransactionState(dirty ? 'dirty' : 'verified');
  }

  return {
    apply,
    broadcastConfig,
    diff,
    getLiveConfig: () => liveConfig,
    getStagedConfig: () => stagedConfig,
    getState,
    isDirty: () => dirty,
    mergeLocalSlotMeta,
    hydrateAuthoritativeConfig,
    reconcileDevicePatch,
    restoreLocalState,
    rollback,
    setLiveConfig: (next) => {
      liveConfig = next;
      dirty = shallowDiff(liveConfig ?? {}, stagedConfig ?? {}).length > 0;
      setDraftState(dirty ? 'dirty' : 'clean');
    },
    setLocalSlotMeta,
    setStagedConfig: (next) => {
      stagedConfig = next;
      dirty = shallowDiff(liveConfig ?? {}, stagedConfig ?? {}).length > 0;
      setDraftState(dirty ? 'dirty' : 'clean');
    },
    resynchronize: () => resynchronizeAfterUncertain('operator-request'),
    stage,
    syncFromSession,
    syncFromDevice
  };
}
