const crypto = require('node:crypto');
const { EventEmitter } = require('node:events');

const { loadSchemaAuthority } = require('./schema_authority');
const { createStructuredEvent } = require('./transport_contract');

const APPLY_TIMEOUT_MS = 30000;
const APPLY_WRITE_TIMEOUT_MS = 5000;
const HANDSHAKE_TIMEOUT_MS = 8000;
const MAX_HANDSHAKE_RETRIES = 2;
const NATIVE_SET_ALL_CHUNK_SIZE = 96;
const NATIVE_SET_ALL_LINE_PACE_MS = 4;
const UNCERTAIN_READBACK_RETRY_MS = 1000;
const MAX_UNCERTAIN_READBACK_RETRIES = 3;

function clone(value) {
  return value == null ? value : JSON.parse(JSON.stringify(value));
}

function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function withTimeout(operation, timeoutMs, message) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(message)), timeoutMs);
    timer.unref?.();
    Promise.resolve()
      .then(operation)
      .then(
        (value) => {
          clearTimeout(timer);
          resolve(value);
        },
        (error) => {
          clearTimeout(timer);
          reject(error);
        },
      );
  });
}

function chunkString(text, chunkSize) {
  const chunks = [];
  for (let index = 0; index < text.length; index += chunkSize) {
    chunks.push(text.slice(index, index + chunkSize));
  }
  return chunks;
}

function isObject(value) {
  return Boolean(value) && typeof value === 'object' && !Array.isArray(value);
}

function isManifestPayload(msg) {
  return (
    isObject(msg) &&
    typeof msg.device_name === 'string' &&
    Number.isFinite(Number(msg.slot_count)) &&
    Number.isFinite(Number(msg.pot_count))
  );
}

function isSchemaPayload(msg) {
  return (
    isObject(msg) &&
    (typeof msg.$schema === 'string' ||
      msg.type === 'object' ||
      isObject(msg.properties))
  );
}

function isConfigPayload(msg) {
  return (
    isObject(msg) &&
    Array.isArray(msg.pots) &&
    Array.isArray(msg.slots) &&
    isObject(msg.led)
  );
}

function fnv1aUtf8(value) {
  let hash = 2166136261;
  for (const byte of Buffer.from(value, 'utf8')) {
    hash ^= byte;
    hash = Math.imul(hash, 16777619);
  }
  return hash >>> 0;
}

function createSessionError(
  code,
  message,
  details = undefined,
  statusCode = 400,
) {
  const error = new Error(message);
  error.code = code;
  error.details = details;
  error.statusCode = statusCode;
  return error;
}

function extractPowerSafety(manifest) {
  return {
    power_profile: manifest?.power_profile ?? null,
    led_brightness_cap: manifest?.led_brightness_cap ?? null,
    rail_topology_verified: manifest?.rail_topology_verified ?? null,
  };
}

function extractHardwareHealth(manifest) {
  return {
    display_present: manifest?.display_present ?? null,
    display_ok: manifest?.display_ok ?? null,
    display_init_failures: manifest?.display_init_failures ?? null,
    display_status: manifest?.display_status ?? null,
    brownout_count: manifest?.brownout_count ?? null,
    eeprom_primary_valid: manifest?.eeprom_primary_valid ?? null,
    eeprom_backup_valid: manifest?.eeprom_backup_valid ?? null,
    eeprom_last_load: manifest?.eeprom_last_load ?? null,
    free_ram: manifest?.free_ram ?? null,
    free_flash: manifest?.free_flash ?? null,
  };
}

function extractFirmwareIdentity(manifest) {
  return {
    device_name: manifest?.device_name ?? null,
    fw_version: manifest?.fw_version ?? null,
    git_sha: manifest?.git_sha ?? null,
    build_time: manifest?.build_time ?? null,
    schema_version: manifest?.schema_version ?? null,
    slot_count: manifest?.slot_count ?? null,
    pot_count: manifest?.pot_count ?? null,
    envelope_count: manifest?.envelope_count ?? null,
    led_count: manifest?.led_count ?? null,
  };
}

function createDeviceSession({
  sendLine,
  writeApplyLine = sendLine,
  abortBulkFrame = () => sendLine('ABORT_SET_ALL'),
  onStateChange = () => {},
  onStructuredEvent = () => {},
  nextSeq = () => 1,
  handshakeTimeoutMs = HANDSHAKE_TIMEOUT_MS,
} = {}) {
  if (typeof sendLine !== 'function') {
    throw new Error('device session requires sendLine');
  }
  if (typeof writeApplyLine !== 'function') {
    throw new Error('device session writeApplyLine must be a function');
  }
  if (typeof abortBulkFrame !== 'function') {
    throw new Error('device session abortBulkFrame must be a function');
  }

  const events = new EventEmitter();
  const state = {
    sessionId: null,
    connected: false,
    handshakeState: 'disconnected',
    helloSeen: false,
    ready: false,
    schemaSource: 'pending',
    manifest: null,
    schema: null,
    liveConfig: null,
    validationAuthority: null,
    compatibility: { status: 'pending', reason: null },
    stagedConfig: null,
    dirty: false,
    sessionRevision: 0,
    lastApplyResult: null,
    powerSafety: extractPowerSafety(null),
    hardwareHealth: extractHardwareHealth(null),
    firmwareIdentity: extractFirmwareIdentity(null),
    lastError: null,
  };

  let authority = null;
  let authorityPromise = null;
  let applyPending = null;
  let uncertainApply = null;
  let handshakeTimer = null;
  let handshakeRetryCount = 0;
  let configRequestCommand = 'GET_CONFIG';
  let configRequested = false;
  let chunkedConfigRead = null;
  let uncertainReadbackTimer = null;
  let uncertainReadbackAttempts = 0;

  function clearUncertainReadbackTimer() {
    if (!uncertainReadbackTimer) return;
    clearTimeout(uncertainReadbackTimer);
    uncertainReadbackTimer = null;
  }

  function requestUncertainReadback() {
    if (!uncertainApply || !state.connected) return;
    try {
      sendLine(state.manifest?.capabilities?.chunked_reads?.config ? 'GET_CONFIG_CHUNKED' : 'GET_CONFIG');
    } catch (error) {
      state.lastError = `Apply outcome unresolved: ${error.message || String(error)}`;
      emitState();
    }
  }

  function scheduleUncertainReadbackRetry() {
    clearUncertainReadbackTimer();
    if (!uncertainApply || !state.connected) return;
    if (uncertainReadbackAttempts >= MAX_UNCERTAIN_READBACK_RETRIES) {
      state.handshakeState = 'unresolved';
      state.lastError = 'Apply outcome unresolved after readback retries; reconnect or retry device readback.';
      setApplyResult({ status: 'unresolved', ...uncertainApply, at: new Date().toISOString() });
      emitState();
      emitStructured('device.apply.unresolved', { ...uncertainApply, attempts: uncertainReadbackAttempts });
      return;
    }
    uncertainReadbackTimer = setTimeout(() => {
      uncertainReadbackTimer = null;
      uncertainReadbackAttempts += 1;
      requestUncertainReadback();
      scheduleUncertainReadbackRetry();
    }, UNCERTAIN_READBACK_RETRY_MS);
  }

  function clearHandshakeTimer() {
    if (!handshakeTimer) return;
    clearTimeout(handshakeTimer);
    handshakeTimer = null;
  }

  function scheduleHandshakeTimeout() {
    clearHandshakeTimer();
    handshakeTimer = setTimeout(() => {
      if (state.ready || !state.connected) return;
      const waitingFor = state.handshakeState;
      const requestByState = {
        'hello-wait': 'HELLO',
        'manifest-wait': 'GET_MANIFEST',
        'schema-wait': 'GET_SCHEMA',
        'config-wait': configRequestCommand,
      };
      const retry = requestByState[waitingFor];
      if (retry && handshakeRetryCount < MAX_HANDSHAKE_RETRIES) {
        handshakeRetryCount += 1;
        state.lastError = `Handshake waiting for ${waitingFor}; retrying ${retry} (${handshakeRetryCount}/${MAX_HANDSHAKE_RETRIES})`;
        emitState();
        emitStructured('bridge.alert', {
          code: 'handshake_retry',
          severity: 'warn',
          message: state.lastError,
        });
        sendLine(retry);
        scheduleHandshakeTimeout();
        return;
      }
      state.handshakeState = 'timeout';
      state.lastError = `Handshake timed out while ${waitingFor}`;
      emitState();
      emitStructured('bridge.alert', {
        code: 'handshake_timeout',
        severity: 'error',
        message: 'Device handshake timed out; reconnect or verify config-mode firmware.',
      });
    }, handshakeTimeoutMs);
  }

  function getState() {
    return clone(state);
  }

  function emitStructured(eventName, payload) {
    const event = createStructuredEvent(eventName, payload);
    events.emit('structured-event', event);
    onStructuredEvent(event);
    return event;
  }

  function emitState() {
    const snapshot = getState();
    events.emit('state', snapshot);
    onStateChange(snapshot);
  }

  function syncIdentityFromManifest() {
    state.powerSafety = extractPowerSafety(state.manifest);
    state.hardwareHealth = extractHardwareHealth(state.manifest);
    state.firmwareIdentity = extractFirmwareIdentity(state.manifest);
  }

  function updateReadyState() {
    const previousReady = state.ready;
    state.ready = Boolean(
      state.connected &&
        state.helloSeen &&
        state.manifest &&
        state.schema &&
        state.compatibility?.status === 'verified' &&
        state.liveConfig,
    );
    return !previousReady && state.ready;
  }

  async function ensureAuthority() {
    if (authority) return authority;
    if (!authorityPromise) {
      authorityPromise = loadSchemaAuthority();
    }
    authority = await authorityPromise;
    state.validationAuthority = {
      source: 'bundled-app',
      schemaVersion: authority.schemaVersion ?? null,
    };
    return authority;
  }

  function evaluateSchemaCompatibility(reportedSchema) {
    const requiredRoots = ['slots', 'efSlots', 'filter', 'arg', 'led'];
    const hasRoots = requiredRoots.every(
      (key) => reportedSchema?.properties?.[key] && reportedSchema?.required?.includes?.(key),
    );
    const reportedVersion = reportedSchema?.schema_version ?? null;
    if (!hasRoots) return { status: 'incompatible', reason: 'missing_required_roots' };
    if (authority?.schemaVersion != null && reportedVersion != null && Number(reportedVersion) !== Number(authority.schemaVersion)) {
      return { status: 'incompatible', reason: 'schema_version_mismatch' };
    }
    return { status: 'verified', reason: null };
  }

  function setApplyResult(result) {
    state.lastApplyResult = clone(result);
  }

  function emitConfigState() {
    state.sessionRevision += 1;
    const snapshot = {
      sessionRevision: state.sessionRevision,
      liveConfig: state.liveConfig,
      stagedConfig: state.stagedConfig,
      dirty: state.dirty,
      lastApplyResult: state.lastApplyResult,
    };
    // Consumers that understand revisions receive one atomic staging truth.
    // The individual events remain for older App/Bridge consumers.
    emitStructured('device.session.snapshot', snapshot);
    emitStructured('device.config.live', {
      config: state.liveConfig,
      lastApplyResult: state.lastApplyResult,
      sessionRevision: state.sessionRevision,
    });
    emitStructured('device.config.staged', {
      config: state.stagedConfig,
      sessionRevision: state.sessionRevision,
    });
    emitStructured('device.config.dirty', {
      dirty: state.dirty,
      sessionRevision: state.sessionRevision,
    });
  }

  function requireSessionRevision(expectedSessionRevision) {
    if (expectedSessionRevision === undefined || expectedSessionRevision === null) return;
    if (Number(expectedSessionRevision) === state.sessionRevision) return;
    throw createSessionError(
      'stale_session_revision',
      'Bridge staged configuration changed in another client; refresh before writing.',
      { expectedSessionRevision: Number(expectedSessionRevision), sessionRevision: state.sessionRevision },
      409,
    );
  }

  function replaceLiveAndStaged(config) {
    state.liveConfig = clone(config);
    state.stagedConfig = clone(config);
    state.dirty = false;
    const becameReady = updateReadyState();
    emitState();
    emitConfigState();
    if (becameReady) {
      clearHandshakeTimer();
      handshakeRetryCount = 0;
      state.handshakeState = 'ready';
      emitStructured('device.ready', {
        manifest: state.manifest,
        schemaSource: state.schemaSource,
        firmwareIdentity: state.firmwareIdentity,
        powerSafety: state.powerSafety,
        hardwareHealth: state.hardwareHealth,
      });
    }
  }

  async function startHandshake() {
    await ensureAuthority();
    handshakeRetryCount = 0;
    scheduleHandshakeTimeout();
    sendLine('HELLO');
    sendLine('GET_MANIFEST');
    sendLine('GET_SCHEMA');
  }

  function requestInitialConfig() {
    if (configRequested) return;
    configRequested = true;
    configRequestCommand = state.manifest?.capabilities?.chunked_reads?.config
      ? 'GET_CONFIG_CHUNKED'
      : 'GET_CONFIG';
    state.handshakeState = 'config-wait';
    sendLine(configRequestCommand);
  }

  function consumeChunkedConfig(msg) {
    if (msg?.type !== 'read_chunk' || msg.command !== 'GET_CONFIG') return null;
    const index = Number(msg.index);
    const total = Number(msg.total);
    const checksum = Number(msg.checksum);
    if (
      !Number.isInteger(index) || !Number.isInteger(total) || !Number.isSafeInteger(checksum) ||
      index < 0 || total < 1 || total > 16384 || index >= total || typeof msg.data !== 'string'
    ) {
      return { error: 'Malformed chunked config response' };
    }
    if (!chunkedConfigRead) {
      chunkedConfigRead = { total, checksum: checksum >>> 0, chunks: new Array(total), count: 0 };
    }
    if (chunkedConfigRead.total !== total || chunkedConfigRead.checksum !== (checksum >>> 0)) {
      chunkedConfigRead = null;
      return { error: 'Inconsistent chunked config response' };
    }
    if (chunkedConfigRead.chunks[index] !== undefined && chunkedConfigRead.chunks[index] !== msg.data) {
      chunkedConfigRead = null;
      return { error: 'Conflicting duplicate config chunk' };
    }
    if (chunkedConfigRead.chunks[index] === undefined) {
      chunkedConfigRead.chunks[index] = msg.data;
      chunkedConfigRead.count += 1;
    }
    if (chunkedConfigRead.count !== chunkedConfigRead.total) return { pending: true };
    const payload = chunkedConfigRead.chunks.join('');
    const expectedChecksum = chunkedConfigRead.checksum;
    chunkedConfigRead = null;
    if (fnv1aUtf8(payload) !== expectedChecksum) return { error: 'Chunked config checksum mismatch' };
    try {
      return { config: JSON.parse(payload) };
    } catch {
      return { error: 'Chunked config response contained invalid JSON' };
    }
  }

  async function handleOpen() {
    // Every serial open is a distinct authority epoch. Never let a prior device's
    // schema or apply receipt satisfy readiness for the newly opened transport.
    state.sessionId = crypto.randomUUID();
    state.connected = true;
    handshakeRetryCount = 0;
    state.handshakeState = 'hello-wait';
    state.helloSeen = false;
    state.ready = false;
    state.manifest = null;
    state.schema = null;
    state.schemaSource = 'pending';
    state.compatibility = { status: 'pending', reason: null };
    state.liveConfig = null;
    state.stagedConfig = null;
    state.dirty = false;
    state.lastApplyResult = null;
    state.lastError = null;
    configRequestCommand = 'GET_CONFIG';
    configRequested = false;
    chunkedConfigRead = null;
    syncIdentityFromManifest();
    emitState();
    await startHandshake();
  }

  function finishRollback(reason, details = {}) {
    state.stagedConfig = clone(state.liveConfig);
    state.dirty = false;
    setApplyResult({
      status: 'rollback',
      reason,
      at: new Date().toISOString(),
      ...clone(details),
    });
    emitState();
    emitConfigState();
    emitStructured('device.apply.rollback', {
      reason,
      lastApplyResult: state.lastApplyResult,
    });
  }

  function markApplyUncertain(reason, pending, error, { attemptReadback = true } = {}) {
    if (!pending) return;
    pending.cancelled = true;
    clearTimeout(pending.timer);
    applyPending = null;
    uncertainApply = { checksum: pending.checksum, seq: pending.seq, reason };
    uncertainReadbackAttempts = 0;
    state.ready = false;
    state.handshakeState = 'resynchronizing';
    state.lastError = `Apply outcome uncertain: ${reason}; reading device state`;
    setApplyResult({ status: 'uncertain', reason, checksum: pending.checksum, seq: pending.seq, at: new Date().toISOString() });
    emitState();
    emitStructured('device.apply.uncertain', { reason, checksum: pending.checksum, seq: pending.seq });
    pending.reject(error);
    if (attemptReadback && state.connected) requestUncertainReadback();
    if (attemptReadback) scheduleUncertainReadbackRetry();
  }

  function rejectPendingApply(error) {
    if (!applyPending) return;
    const pending = applyPending;
    applyPending = null;
    pending.cancelled = true;
    clearTimeout(pending.timer);
    pending.reject(error);
  }

  function handleDisconnect(reason = 'disconnect') {
    clearUncertainReadbackTimer();
    clearHandshakeTimer();
    handshakeRetryCount = 0;
    if (applyPending) {
      const pending = applyPending;
      markApplyUncertain(reason, pending,
        createSessionError(
          'apply_interrupted',
          'Device disconnected during staged apply',
          { reason, checksum: pending.checksum, seq: pending.seq },
          503,
        ), { attemptReadback: false });
    }
    state.connected = false;
    state.handshakeState = 'disconnected';
    state.helloSeen = false;
    state.ready = false;
    state.lastError = reason === 'disconnect' ? null : reason;
    emitState();
  }

  function handleMalformedMessage(line, error) {
    if (
      String(line || '')
        .trim()
        .startsWith('{')
    ) {
      const alertPayload = {
        code: 'malformed_device_response',
        severity: 'warn',
        message: 'Device returned malformed JSON',
        details: {
          line: String(line).slice(0, 200),
          error: error?.message ?? 'parse failed',
        },
      };
      emitStructured('bridge.alert', alertPayload);
    }
  }

  function completePendingApply(pending, verifiedConfig = pending.stagedConfig) {
    const appliedAt = new Date().toISOString();
    state.liveConfig = clone(verifiedConfig);
    state.stagedConfig = clone(verifiedConfig);
    state.dirty = false;
    setApplyResult({
      status: 'ack', checksum: pending.checksum, appliedChecksum: pending.appliedChecksum ?? null,
      storageGeneration: pending.storageGeneration ?? null, seq: pending.seq, at: appliedAt,
    });
    applyPending = null;
    clearTimeout(pending.timer);
    updateReadyState();
    emitState(); emitConfigState();
    emitStructured('device.apply.ack', {
      checksum: pending.checksum, applied_checksum: pending.appliedChecksum ?? null,
      storage_generation: pending.storageGeneration ?? null, seq: pending.seq, appliedAt,
    });
    pending.resolve({ applied: true, checksum: pending.checksum, appliedChecksum: pending.appliedChecksum ?? null,
      storageGeneration: pending.storageGeneration ?? null, seq: pending.seq });
  }

  async function handleMessage(msg, meta = {}) {
    if (!isObject(msg)) return;
    await ensureAuthority();

    const chunkedConfig = consumeChunkedConfig(msg);
    if (chunkedConfig?.error) {
      state.lastError = chunkedConfig.error;
      emitState();
      emitStructured('bridge.alert', {
        code: 'chunked_config_invalid', severity: 'error', message: chunkedConfig.error,
      });
      return;
    }
    if (chunkedConfig?.pending) return;
    if (chunkedConfig?.config) {
      await handleMessage(chunkedConfig.config, meta);
      return;
    }

    if (msg.hello === 'mn42') {
      state.handshakeState = state.manifest ? (state.schema ? 'config-wait' : 'schema-wait') : 'manifest-wait';
      state.helloSeen = true;
      state.lastError = null;
      const becameReady = updateReadyState();
      emitState();
      if (becameReady) {
        emitStructured('device.ready', {
          manifest: state.manifest,
          schemaSource: state.schemaSource,
          firmwareIdentity: state.firmwareIdentity,
          powerSafety: state.powerSafety,
          hardwareHealth: state.hardwareHealth,
        });
      }
    }

    if (isManifestPayload(msg)) {
      state.handshakeState = state.schema ? 'config-wait' : 'schema-wait';
      state.manifest = clone(msg);
      syncIdentityFromManifest();
      requestInitialConfig();
      const becameReady = updateReadyState();
      emitState();
      if (becameReady) {
        emitStructured('device.ready', {
          manifest: state.manifest,
          schemaSource: state.schemaSource,
          firmwareIdentity: state.firmwareIdentity,
          powerSafety: state.powerSafety,
          hardwareHealth: state.hardwareHealth,
        });
      }
    } else if (
      isObject(msg.result?.manifest) &&
      isManifestPayload(msg.result.manifest)
    ) {
      state.manifest = clone(msg.result.manifest);
      syncIdentityFromManifest();
      requestInitialConfig();
      const becameReady = updateReadyState();
      emitState();
      if (becameReady) {
        emitStructured('device.ready', {
          manifest: state.manifest,
          schemaSource: state.schemaSource,
          firmwareIdentity: state.firmwareIdentity,
          powerSafety: state.powerSafety,
          hardwareHealth: state.hardwareHealth,
        });
      }
    }

    if (isSchemaPayload(msg)) {
      state.handshakeState = state.liveConfig ? 'ready' : 'config-wait';
      state.schema = clone(msg);
      state.schemaSource = 'device';
      state.compatibility = evaluateSchemaCompatibility(msg);
      if (state.compatibility.status === 'incompatible') {
        clearHandshakeTimer();
        handshakeRetryCount = 0;
        state.handshakeState = 'degraded';
        emitStructured('bridge.alert', {
          code: 'device_schema_incompatible',
          severity: 'error',
          message: 'Reported device schema is incompatible with the bundled validation authority.',
          details: clone(state.compatibility),
        });
      }
      const becameReady = updateReadyState();
      emitState();
      if (becameReady) {
        emitStructured('device.ready', {
          manifest: state.manifest,
          schemaSource: state.schemaSource,
          firmwareIdentity: state.firmwareIdentity,
          powerSafety: state.powerSafety,
          hardwareHealth: state.hardwareHealth,
        });
      }
    }

    if (isConfigPayload(msg)) {
      const normalized = authority.normalizeConfig(
        clone(msg),
        state.manifest ?? {},
      );
      if (applyPending?.awaitingReadback) {
        const pending = applyPending;
        if (JSON.stringify(normalized) !== JSON.stringify(pending.stagedConfig)) {
          const error = createSessionError('apply_readback_mismatch',
            'Committed device configuration differs from the staged configuration', null, 409);
          applyPending = null;
          clearTimeout(pending.timer);
          state.liveConfig = clone(normalized);
          state.stagedConfig = clone(normalized);
          state.dirty = false;
          setApplyResult({
            status: 'verified_device_different',
            checksum: pending.checksum,
            seq: pending.seq,
            at: new Date().toISOString(),
          });
          emitState(); emitConfigState();
          pending.reject(error);
          emitStructured('device.apply.device_different', {
            checksum: pending.checksum,
            seq: pending.seq,
            lastApplyResult: state.lastApplyResult,
          });
          return;
        }
        completePendingApply(pending, normalized);
        return;
      }
      replaceLiveAndStaged(normalized);
      if (uncertainApply) {
        clearUncertainReadbackTimer();
        const resolved = uncertainApply;
        uncertainApply = null;
        state.lastError = null;
        setApplyResult({ status: 'resynchronized', ...resolved, at: new Date().toISOString() });
        updateReadyState();
        emitState();
        emitStructured('device.apply.resynchronized', resolved);
      }
    }

    if (
      msg.type === 'telemetry' ||
      Array.isArray(msg.slots) ||
      Array.isArray(msg.envelopes)
    ) {
      emitStructured('device.telemetry', {
        telemetry: clone(msg),
        traceId: meta.traceId ?? null,
        hostTimestampMs: meta.hostTimestampMs ?? null,
        sourceTimestampMs: meta.sourceTimestampMs ?? null,
      });
    }

    if (applyPending && msg.type === 'ack') {
      if (msg.checksum !== applyPending.checksum || msg.seq !== applyPending.seq) {
        const mismatchError = createSessionError(
          'apply_checksum_mismatch',
          'Device ACK checksum did not match the staged payload',
          {
            expected: applyPending.checksum,
            received: msg.checksum ?? null,
            expectedSeq: applyPending.seq,
            seq: msg.seq ?? null,
          },
          409,
        );
        markApplyUncertain('checksum_mismatch', applyPending, mismatchError);
        emitStructured('bridge.alert', {
          code: mismatchError.code,
          severity: 'error',
          message: mismatchError.message,
          details: mismatchError.details,
        });
        return;
      }
      const requiresIntegrityReceipt = state.manifest?.persistence?.backend === 'littlefs';
      if (requiresIntegrityReceipt && (!msg.applied_checksum || msg.storage_generation === undefined)) {
        const receiptError = createSessionError(
          'apply_integrity_receipt_missing',
          'Device ACK omitted the applied-state checksum or storage generation',
          { checksum: msg.checksum, seq: msg.seq ?? null },
          409,
        );
        markApplyUncertain('integrity_receipt_missing', applyPending, receiptError);
        emitStructured('bridge.alert', {
          code: receiptError.code,
          severity: 'error',
          message: receiptError.message,
          details: receiptError.details,
        });
        return;
      }
      if (requiresIntegrityReceipt) {
        applyPending.awaitingReadback = true;
        applyPending.appliedChecksum = msg.applied_checksum;
        applyPending.storageGeneration = msg.storage_generation;
        sendLine(state.manifest?.capabilities?.chunked_reads?.config ? 'GET_CONFIG_CHUNKED' : 'GET_CONFIG');
        return;
      }
      const appliedAt = new Date().toISOString();
      state.liveConfig = clone(applyPending.stagedConfig);
      state.dirty = false;
      setApplyResult({
        status: 'ack',
        checksum: msg.checksum,
        appliedChecksum: msg.applied_checksum ?? null,
        storageGeneration: msg.storage_generation ?? null,
        seq: msg.seq ?? applyPending.seq,
        at: appliedAt,
      });
      const pending = applyPending;
      applyPending = null;
      clearTimeout(pending.timer);
      updateReadyState();
      emitState();
      emitConfigState();
      emitStructured('device.apply.ack', {
        checksum: msg.checksum,
        applied_checksum: msg.applied_checksum ?? null,
        storage_generation: msg.storage_generation ?? null,
        seq: msg.seq ?? pending.seq,
        appliedAt,
      });
      pending.resolve({
        applied: true,
        checksum: msg.checksum,
        appliedChecksum: msg.applied_checksum ?? null,
        storageGeneration: msg.storage_generation ?? null,
        seq: msg.seq ?? pending.seq,
      });
      return;
    }

    if (applyPending && msg.type === 'error') {
      const error = createSessionError(
        `device_${msg.code || 'error'}`,
        msg.message || 'Firmware rejected staged apply',
        clone(msg),
        409,
      );
      finishRollback('device_error', {
        checksum: applyPending.checksum,
        seq: applyPending.seq,
        deviceError: clone(msg),
      });
      rejectPendingApply(error);
      emitStructured('bridge.alert', {
        code: error.code,
        severity: 'error',
        message: error.message,
        details: error.details,
      });
      return;
    }
  }

  async function stageConfig(nextConfig, { expectedSessionRevision } = {}) {
    await ensureAuthority();
    requireSessionRevision(expectedSessionRevision);
    if (applyPending || uncertainApply) {
      throw createSessionError(
        'apply_outcome_unresolved',
        'Cannot replace the staged config until the transmitted Apply outcome has been resynchronized.',
        {
          checksum: applyPending?.checksum ?? uncertainApply?.checksum,
          seq: applyPending?.seq ?? uncertainApply?.seq,
        },
        409,
      );
    }
    if (!state.manifest) {
      throw createSessionError(
        'device_not_ready',
        'Cannot stage config before the device manifest is cached',
        null,
        503,
      );
    }
    const normalized = authority.normalizeConfig(
      clone(nextConfig),
      state.manifest,
    );
    const validation = authority.validateConfig(normalized);
    if (!validation.valid) {
      throw createSessionError(
        'schema_validation_failed',
        'Staged config failed schema validation',
        { errors: validation.errors },
        422,
      );
    }
    state.stagedConfig = clone(normalized);
    state.dirty =
      JSON.stringify(state.liveConfig ?? null) !==
      JSON.stringify(state.stagedConfig ?? null);
    emitState();
    emitConfigState();
    return {
      staged: clone(state.stagedConfig),
      dirty: state.dirty,
      sessionRevision: state.sessionRevision,
    };
  }

  async function rollback(reason = 'operator_request') {
    if (applyPending || uncertainApply) {
      throw createSessionError(
        'apply_outcome_unresolved',
        'Cannot roll back until the transmitted Apply outcome has been resynchronized.',
        {
          checksum: applyPending?.checksum ?? uncertainApply?.checksum,
          seq: applyPending?.seq ?? uncertainApply?.seq,
        },
        409,
      );
    }
    finishRollback(reason);
    return {
      rolledBack: true,
      reason,
      state: getState(),
    };
  }

  async function applyStagedConfig({
    timeoutMs = APPLY_TIMEOUT_MS,
    writeTimeoutMs = APPLY_WRITE_TIMEOUT_MS,
    expectedSessionRevision
  } = {}) {
    await ensureAuthority();
    requireSessionRevision(expectedSessionRevision);
    if (!state.connected) {
      throw createSessionError(
        'device_not_connected',
        'Cannot apply config while the serial device is disconnected',
        null,
        503,
      );
    }
    if (
      !state.manifest ||
      !state.schema ||
      !state.liveConfig ||
      !state.stagedConfig
    ) {
      throw createSessionError(
        'device_not_ready',
        'Cannot apply until handshake, schema, and config cache are ready',
        null,
        503,
      );
    }
    if (state.compatibility?.status !== 'verified') {
      throw createSessionError(
        'device_schema_incompatible',
        'Cannot apply because the reported device schema is not compatible with the bundled validation authority',
        clone(state.compatibility),
        409,
      );
    }
    if (!state.dirty) {
      return {
        applied: false,
        reason: 'clean',
        state: getState(),
      };
    }
    if (applyPending) {
      throw createSessionError(
        'apply_in_progress',
        'A staged apply is already in progress',
        { checksum: applyPending.checksum, seq: applyPending.seq },
        409,
      );
    }

    const validation = authority.validateConfig(state.stagedConfig);
    if (!validation.valid) {
      throw createSessionError(
        'schema_validation_failed',
        'Staged config failed schema validation',
        { errors: validation.errors },
        422,
      );
    }

    const seq = nextSeq();
    const body = JSON.stringify({
      seq,
      schema_version:
        state.schema?.schema_version ??
        authority.schemaVersion ??
        state.manifest?.schema_version ??
        null,
      manifest: {
        fw_version: state.manifest?.fw_version ?? null,
        git_sha: state.manifest?.git_sha ?? null,
        build_time: state.manifest?.build_time ?? null,
        schema_version: state.manifest?.schema_version ?? null,
      },
      config: state.stagedConfig,
      deviceConfig: authority.compactConfigForDevice(
        state.stagedConfig,
        state.liveConfig,
        {
          clone,
          slotTypeNames: authority.slotTypeNames,
        },
      ),
    });
    const checksum = crypto.createHash('sha256').update(body).digest('hex');
    const nativePayload = JSON.stringify({
      seq,
      checksum,
      config: authority.compactConfigForDevice(
        state.stagedConfig,
        state.liveConfig,
        {
          clone,
          slotTypeNames: authority.slotTypeNames,
        },
      ),
    });
    const lines = chunkString(nativePayload, NATIVE_SET_ALL_CHUNK_SIZE).map(
      (chunk) => `SET_ALL ${chunk}`,
    );

    setApplyResult({
      status: 'pending',
      checksum,
      seq,
      at: new Date().toISOString(),
    });
    emitState();

    return new Promise((resolve, reject) => {
      const pending = {
        checksum,
        seq,
        stagedConfig: clone(state.stagedConfig),
        timer: null,
        cancelled: false,
        resolve,
        reject,
      };
      const timer = setTimeout(() => {
        if (applyPending !== pending) return;
        markApplyUncertain('timeout', pending, createSessionError(
            'apply_timeout',
            'Timed out waiting for device apply ACK',
            { checksum, seq, timeoutMs },
            504,
          ));
      }, timeoutMs);

      pending.timer = timer;
      applyPending = pending;

      (async () => {
        try {
          for (let index = 0; index < lines.length; index += 1) {
            await withTimeout(
              () => writeApplyLine(lines[index]),
              writeTimeoutMs,
              `Timed out writing Apply frame after ${writeTimeoutMs}ms`,
            );
            if (pending.cancelled) {
              throw new Error('Apply serial ownership ended before the payload write completed');
            }
            if (index + 1 < lines.length) {
              await delay(NATIVE_SET_ALL_LINE_PACE_MS);
              if (pending.cancelled) {
                throw new Error('Apply serial ownership ended before the payload write completed');
              }
            }
          }
        } catch (error) {
          // An expired writer retains its cancellation token even after public
          // uncertainty is cleared. It must not abort or resume into a newer
          // serial ownership epoch when its callback eventually settles.
          if (pending.cancelled) return;
          try {
            // This is the owning Apply transaction releasing its own partial
            // frame. Bridge implementations may bypass their public live
            // command queue here without allowing another client to abort it.
            await abortBulkFrame();
          } catch {
            // A disconnected transport cannot receive the abort; firmware's
            // autonomous assembler timeout clears the partial frame.
          }
          markApplyUncertain('transport_error', pending, createSessionError(
              'apply_transport_error',
              error?.message || 'Failed to write staged apply payload',
              { checksum, seq },
              503,
            ));
        }
      })();
    });
  }

  function getApiState() {
    return {
      session: getState(),
    };
  }

  function on(eventName, handler) {
    events.on(eventName, handler);
    return () => events.off(eventName, handler);
  }

  return {
    applyStagedConfig,
    ensureAuthority,
    getApiState,
    getState,
    handleDisconnect,
    handleMalformedMessage,
    handleMessage,
    handleOpen,
    isApplyTransactionActive: () => Boolean(applyPending || uncertainApply),
    on,
    rollback,
    stageConfig,
  };
}

module.exports = {
  APPLY_TIMEOUT_MS,
  NATIVE_SET_ALL_CHUNK_SIZE,
  NATIVE_SET_ALL_LINE_PACE_MS,
  createDeviceSession,
  createSessionError,
  extractFirmwareIdentity,
  extractPowerSafety,
  isConfigPayload,
  isManifestPayload,
  isSchemaPayload,
};
