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

function isDefaultSlotEfForDevice(ef, efIndex) {
  if (!ef || typeof ef !== 'object') return true;
  const expectedIndex = Number.isFinite(Number(efIndex)) ? Math.round(Number(efIndex)) : -1;
  const expected = {
    index: expectedIndex,
    filter_index: 0,
    filter_name: 'LINEAR',
    frequency: 1000,
    q: 0.707,
    oversample: 4,
    smoothing: 0.2,
    baseline: 0,
    gain: 1,
    destination_mode: 'add_clamp'
  };
  return Object.entries(expected).every(([key, value]) => {
    if (ef[key] === undefined) return true;
    if (typeof value === 'number') return Math.abs(Number(ef[key]) - value) < 0.00001;
    return ef[key] === value;
  });
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
  if (slot?.arpNote !== undefined) out.arpNote = clone(slot.arpNote);
  out.active = Boolean(slot?.active);
  out.ef_index = resolveEfIndexForDevice(slot);
  if (slot?.sysexTemplate !== undefined && slot?.sysexTemplate !== previousSlot?.sysexTemplate) {
    out.sysexTemplate = clone(slot.sysexTemplate);
  }
  const efIndexForDevice = out.ef_index ?? slot?.efIndex ?? slot?.ef_index ?? slot?.ef?.index;
  if (
    slot?.ef &&
    typeof slot.ef === 'object' &&
    !isDefaultSlotEfForDevice(slot.ef, efIndexForDevice) &&
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
  rollbackBridgeConfig = null
} = {}) {
  let liveConfig = null;
  let stagedConfig = null;
  let dirty = false;
  let lastKnownChecksum = null;
  let transactionState = 'clean';
  let attemptedApply = null;

  function setTransactionState(next, details = {}) {
    transactionState = next;
    emit('config-transaction', { state: next, ...details });
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
    stagedConfig = clone(normalized);
    dirty = false;
    setTransactionState('verified');
  }

  function syncFromSession(sessionPayload = {}) {
    const normalizedLive = normalizeConfig(sessionPayload.liveConfig ?? {}, getManifest());
    const normalizedStaged = normalizeConfig(
      sessionPayload.stagedConfig ?? sessionPayload.liveConfig ?? {},
      getManifest()
    );
    liveConfig = clone(normalizedLive);
    stagedConfig = clone(normalizedStaged);
    if (sessionPayload.dirty === undefined) {
      dirty = shallowDiff(normalizedLive ?? {}, normalizedStaged ?? {}).length > 0;
    } else {
      dirty = Boolean(sessionPayload.dirty);
    }
    setTransactionState(dirty ? 'dirty' : 'verified');
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
    setTransactionState(dirty ? 'dirty' : 'clean');
    broadcastConfig();
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
      liveConfig = clone(deviceConfig);
      stagedConfig = clone(deviceConfig);
      dirty = false;
      setTransactionState('verified', { reason: 'resynchronized', attemptedApply });
      broadcastConfig();
      emit('resynchronized', { attemptedApply });
      return deviceConfig;
    } catch (resyncError) {
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

  async function apply() {
    if (!dirty) return { applied: false };
    if (['uncertain', 'resynchronizing'].includes(transactionState)) {
      throw new Error('Apply is blocked until the previous transmitted configuration is resynchronized.');
    }
    const validator = getValidator();
    if (!validator(stagedConfig)) {
      const error = new Error('Schema validation failed');
      error.validation = validator.errors;
      emit('validation-error', validator.errors);
      throw error;
    }
    if (isBridgeSessionActive()) {
      try {
        const response =
          typeof applyBridgeConfig === 'function' ? await applyBridgeConfig() : { applied: false };
        const checksum = response?.checksum ?? response?.result?.checksum ?? null;
        liveConfig = clone(stagedConfig);
        dirty = false;
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
      config: clone(stagedConfig),
      deviceConfig: compactConfigForDevice(stagedConfig, liveConfig, {
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
      await markApplyUncertain('transport-failure', payload, checksum);
      if (/RPC timeout/i.test(err?.message ?? '')) {
        throw new Error('Timed out waiting for firmware ACK; device state is being resynchronized.');
      }
      throw err;
    }
    const ackChecksum = response?.checksum ?? response?.result?.checksum ?? null;
    const appliedChecksum = response?.applied_checksum ?? response?.result?.applied_checksum ?? null;
    const storageGeneration = response?.storage_generation ?? response?.result?.storage_generation ?? null;
    if (ackChecksum !== checksum) {
      await markApplyUncertain('malformed-ack', payload, checksum);
      throw new Error('Device failed to acknowledge apply');
    }
    // Hardware bulk Apply is durable only when firmware confirms its own
    // applied-state digest and committed storage generation. Simulators and
    // older manifests retain their compatibility path.
    const requiresHardwareIntegrity = remoteManifest?.persistence?.backend === 'littlefs';
    if (requiresHardwareIntegrity && (!appliedChecksum || storageGeneration === null)) {
      await markApplyUncertain('missing-integrity-receipt', payload, checksum);
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
      const expected = normalizeConfig(stagedConfig, getManifest());
      if (JSON.stringify(readback) !== JSON.stringify(expected)) {
        // The device is the authority after an ACK. Preserve its truth instead
        // of promoting an unverified browser-side candidate.
        liveConfig = clone(readback);
        stagedConfig = clone(readback);
        dirty = false;
        setTransactionState('verified', { reason: 'readback-mismatch', attemptedApply: { seq: payload.seq, checksum } });
        broadcastConfig();
        throw new Error('Device readback differs from the applied configuration');
      }
    }
    liveConfig = clone(stagedConfig);
    dirty = false;
    lastKnownChecksum = checksum;
    attemptedApply = null;
    setTransactionState('verified', { seq: payload.seq, checksum });
    broadcastConfig();
    emit('applied', { checksum, appliedChecksum, storageGeneration });
    return { applied: true, checksum, appliedChecksum, storageGeneration };
  }

  async function rollback() {
    if (['uncertain', 'resynchronizing'].includes(transactionState)) {
      throw new Error('Cannot discard a transmitted configuration while device state is uncertain. Resynchronize first.');
    }
    if (isBridgeSessionActive() && typeof rollbackBridgeConfig === 'function') {
      await rollbackBridgeConfig('operator_request');
    }
    stagedConfig = clone(liveConfig);
    dirty = false;
    setTransactionState('clean');
    broadcastConfig();
    emit('rollback', {});
  }

  function restoreLocalState({ allowDifferentFirmware = false } = {}) {
    const snapshot = stateSnapshotStore.read?.();
    const identity = stateSnapshotStore.identityDecision?.(snapshot);
    if (identity === 'different-firmware' && !allowDifferentFirmware) {
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

  function replaceConfig(configPayload) {
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
      attemptedApply: attemptedApply ? clone(attemptedApply) : null
    };
  }

  function setLocalSlotMeta(index, patch = {}) {
    if (!localSlotMetaManager.updateEntry(index, patch)) return false;
    broadcastConfig();
    return true;
  }

  function reconcileDevicePatch(nextLive, nextStaged) {
    liveConfig = clone(nextLive);
    stagedConfig = clone(nextStaged);
    dirty = shallowDiff(liveConfig ?? {}, stagedConfig ?? {}).length > 0;
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
    replaceConfig,
    reconcileDevicePatch,
    restoreLocalState,
    rollback,
    setLiveConfig: (next) => {
      liveConfig = next;
      dirty = shallowDiff(liveConfig ?? {}, stagedConfig ?? {}).length > 0;
    },
    setLocalSlotMeta,
    setStagedConfig: (next) => {
      stagedConfig = next;
      dirty = shallowDiff(liveConfig ?? {}, stagedConfig ?? {}).length > 0;
    },
    resynchronize: () => resynchronizeAfterUncertain('operator-request'),
    stage,
    syncFromSession,
    syncFromDevice
  };
}
