const crypto = require('node:crypto');
const { EventEmitter } = require('node:events');

const { loadSchemaAuthority } = require('./schema_authority');
const { createStructuredEvent } = require('./transport_contract');

const APPLY_TIMEOUT_MS = 30000;
const NATIVE_SET_ALL_CHUNK_SIZE = 96;
const NATIVE_SET_ALL_LINE_PACE_MS = 4;

function clone(value) {
  return value == null ? value : JSON.parse(JSON.stringify(value));
}

function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
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
  onStateChange = () => {},
  onStructuredEvent = () => {},
  nextSeq = () => 1,
} = {}) {
  if (typeof sendLine !== 'function') {
    throw new Error('device session requires sendLine');
  }

  const events = new EventEmitter();
  const state = {
    connected: false,
    helloSeen: false,
    ready: false,
    schemaSource: 'bundled',
    manifest: null,
    schema: null,
    liveConfig: null,
    stagedConfig: null,
    dirty: false,
    lastApplyResult: null,
    powerSafety: extractPowerSafety(null),
    firmwareIdentity: extractFirmwareIdentity(null),
    lastError: null,
  };

  let authority = null;
  let authorityPromise = null;
  let applyPending = null;

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
    state.firmwareIdentity = extractFirmwareIdentity(state.manifest);
  }

  function updateReadyState() {
    const previousReady = state.ready;
    state.ready = Boolean(
      state.connected &&
        state.helloSeen &&
        state.manifest &&
        state.schema &&
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
    if (!state.schema) {
      state.schema = clone(authority.schema);
      state.schemaSource = 'bundled';
      updateReadyState();
      emitState();
    }
    return authority;
  }

  function setApplyResult(result) {
    state.lastApplyResult = clone(result);
  }

  function emitConfigState() {
    emitStructured('device.config.live', {
      config: state.liveConfig,
      lastApplyResult: state.lastApplyResult,
    });
    emitStructured('device.config.staged', {
      config: state.stagedConfig,
    });
    emitStructured('device.config.dirty', {
      dirty: state.dirty,
    });
  }

  function replaceLiveAndStaged(config) {
    state.liveConfig = clone(config);
    state.stagedConfig = clone(config);
    state.dirty = false;
    const becameReady = updateReadyState();
    emitState();
    emitConfigState();
    if (becameReady) {
      emitStructured('device.ready', {
        manifest: state.manifest,
        schemaSource: state.schemaSource,
        firmwareIdentity: state.firmwareIdentity,
        powerSafety: state.powerSafety,
      });
    }
  }

  async function startHandshake() {
    await ensureAuthority();
    sendLine('HELLO');
    sendLine('GET_MANIFEST');
    sendLine('GET_SCHEMA');
    sendLine('GET_CONFIG');
  }

  async function handleOpen() {
    state.connected = true;
    state.helloSeen = false;
    state.ready = false;
    state.manifest = null;
    state.liveConfig = null;
    state.stagedConfig = null;
    state.dirty = false;
    state.lastError = null;
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

  function rejectPendingApply(error) {
    if (!applyPending) return;
    const pending = applyPending;
    applyPending = null;
    clearTimeout(pending.timer);
    pending.reject(error);
  }

  function handleDisconnect(reason = 'disconnect') {
    if (applyPending) {
      finishRollback(reason, {
        checksum: applyPending.checksum,
        seq: applyPending.seq,
      });
      rejectPendingApply(
        createSessionError(
          'apply_interrupted',
          'Device disconnected during staged apply',
          { reason },
          503,
        ),
      );
    }
    state.connected = false;
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

  async function handleMessage(msg, meta = {}) {
    if (!isObject(msg)) return;
    await ensureAuthority();

    if (msg.hello === 'mn42') {
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
        });
      }
    }

    if (isManifestPayload(msg)) {
      state.manifest = clone(msg);
      syncIdentityFromManifest();
      const becameReady = updateReadyState();
      emitState();
      if (becameReady) {
        emitStructured('device.ready', {
          manifest: state.manifest,
          schemaSource: state.schemaSource,
          firmwareIdentity: state.firmwareIdentity,
          powerSafety: state.powerSafety,
        });
      }
    } else if (
      isObject(msg.result?.manifest) &&
      isManifestPayload(msg.result.manifest)
    ) {
      state.manifest = clone(msg.result.manifest);
      syncIdentityFromManifest();
      const becameReady = updateReadyState();
      emitState();
      if (becameReady) {
        emitStructured('device.ready', {
          manifest: state.manifest,
          schemaSource: state.schemaSource,
          firmwareIdentity: state.firmwareIdentity,
          powerSafety: state.powerSafety,
        });
      }
    }

    if (isSchemaPayload(msg)) {
      state.schema = clone(msg);
      state.schemaSource = 'device';
      const becameReady = updateReadyState();
      emitState();
      if (becameReady) {
        emitStructured('device.ready', {
          manifest: state.manifest,
          schemaSource: state.schemaSource,
          firmwareIdentity: state.firmwareIdentity,
          powerSafety: state.powerSafety,
        });
      }
    }

    if (isConfigPayload(msg)) {
      const normalized = authority.normalizeConfig(
        clone(msg),
        state.manifest ?? {},
      );
      replaceLiveAndStaged(normalized);
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
      if (msg.checksum !== applyPending.checksum) {
        const mismatchError = createSessionError(
          'apply_checksum_mismatch',
          'Device ACK checksum did not match the staged payload',
          {
            expected: applyPending.checksum,
            received: msg.checksum ?? null,
            seq: msg.seq ?? null,
          },
          409,
        );
        finishRollback('checksum_mismatch', mismatchError.details);
        rejectPendingApply(mismatchError);
        emitStructured('bridge.alert', {
          code: mismatchError.code,
          severity: 'error',
          message: mismatchError.message,
          details: mismatchError.details,
        });
        return;
      }
      const appliedAt = new Date().toISOString();
      state.liveConfig = clone(state.stagedConfig);
      state.dirty = false;
      setApplyResult({
        status: 'ack',
        checksum: msg.checksum,
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
        seq: msg.seq ?? pending.seq,
        appliedAt,
      });
      pending.resolve({
        applied: true,
        checksum: msg.checksum,
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

  async function stageConfig(nextConfig) {
    await ensureAuthority();
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
    };
  }

  async function rollback(reason = 'operator_request') {
    finishRollback(reason);
    return {
      rolledBack: true,
      reason,
      state: getState(),
    };
  }

  async function applyStagedConfig({ timeoutMs = APPLY_TIMEOUT_MS } = {}) {
    await ensureAuthority();
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
      const timer = setTimeout(() => {
        if (!applyPending || applyPending.checksum !== checksum) return;
        finishRollback('timeout', { checksum, seq });
        applyPending = null;
        reject(
          createSessionError(
            'apply_timeout',
            'Timed out waiting for device apply ACK',
            { checksum, seq, timeoutMs },
            504,
          ),
        );
      }, timeoutMs);

      applyPending = {
        checksum,
        seq,
        timer,
        resolve,
        reject,
      };

      (async () => {
        try {
          for (let index = 0; index < lines.length; index += 1) {
            sendLine(lines[index]);
            if (index + 1 < lines.length) {
              await delay(NATIVE_SET_ALL_LINE_PACE_MS);
            }
          }
        } catch (error) {
          clearTimeout(timer);
          applyPending = null;
          finishRollback('transport_error', { checksum, seq });
          reject(
            createSessionError(
              'apply_transport_error',
              error?.message || 'Failed to write staged apply payload',
              { checksum, seq },
              503,
            ),
          );
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
