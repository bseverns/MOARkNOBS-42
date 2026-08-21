import { MN42_SCHEMA_VERSION } from '../manifest_contract.js';

export const SUPPORTED_BRIDGE_API_VERSIONS = Object.freeze([1]);
export const SUPPORTED_BRIDGE_EVENT_CONTRACT_VERSIONS = Object.freeze([1]);

const PREFLIGHT_REJECTION_CODES = new Set([
  'schema_validation_failed',
  'stale_session_revision',
  'staged_apply_identity_mismatch',
  'staged_digest_mismatch',
  'device_not_ready',
  'device_not_connected',
  'device_schema_incompatible',
  'apply_in_progress'
]);

function classifyBridgeFailure(code) {
  if (PREFLIGHT_REJECTION_CODES.has(code)) return 'preflight-rejected';
  if (typeof code === 'string' && code.startsWith('device_')) {
    return 'device-rejected-before-commit';
  }
  return 'transmission-unknown';
}

function createBridgeError(payload, fallbackMessage) {
  const body = payload?.error ?? payload ?? {};
  const error = new Error(body.message ?? fallbackMessage ?? 'Bridge request failed');
  if (body.code !== undefined) error.code = body.code;
  if (body.details !== undefined) error.details = body.details;
  // New Bridge versions own this classification. The code-based mapping is
  // retained for compatibility with older runtimes.
  error.bridgeFailureClass =
    body.failureClass ?? classifyBridgeFailure(error.code);
  error.bridgeSession = payload?.state?.deviceSession ?? null;
  return error;
}

function createInvalidApplyResponseError(session = null) {
  const error = new Error('Bridge Apply response omitted its transaction result.');
  error.code = 'invalid_bridge_apply_response';
  error.bridgeFailureClass = 'transmission-unknown';
  error.bridgeSession = session;
  return error;
}

function isRecognizedApplyResult(result) {
  return (
    result?.applied === true ||
    (result?.applied === false && result?.reason === 'clean')
  );
}

function resolveUrl(url, base) {
  if (!url) return null;
  try {
    return new URL(url, base).toString();
  } catch (_) {
    return null;
  }
}

function createContractError(code, message, contract = null) {
  const error = new Error(message);
  error.code = code;
  error.bridgeContract = contract;
  return error;
}

export function validateBridgeContract(contract) {
  if (!contract || typeof contract !== 'object' || Array.isArray(contract)) {
    throw createContractError(
      'invalid_bridge_contract',
      'Bridge contract metadata is missing or malformed.',
      contract
    );
  }
  if (!SUPPORTED_BRIDGE_API_VERSIONS.includes(contract.bridge_api_version)) {
    throw createContractError(
      'unsupported_bridge_api_version',
      `Unsupported Bridge API version: ${String(contract.bridge_api_version)}`,
      contract
    );
  }
  if (
    !SUPPORTED_BRIDGE_EVENT_CONTRACT_VERSIONS.includes(
      contract.event_contract_version
    )
  ) {
    throw createContractError(
      'unsupported_bridge_event_contract_version',
      `Unsupported Bridge event contract version: ${String(
        contract.event_contract_version
      )}`,
      contract
    );
  }
  if (
    !Array.isArray(contract.supported_schema_versions) ||
    !contract.supported_schema_versions.includes(MN42_SCHEMA_VERSION)
  ) {
    throw createContractError(
      'unsupported_bridge_schema_version',
      `Bridge does not support App schema version ${MN42_SCHEMA_VERSION}.`,
      contract
    );
  }
  if (contract.verified_apply !== true || contract.structured_session !== true) {
    throw createContractError(
      'unsupported_bridge_capabilities',
      'Bridge does not advertise the verified structured-session contract.',
      contract
    );
  }
  if (
    typeof contract.bridge_version !== 'string' ||
    !contract.bridge_version.trim()
  ) {
    throw createContractError(
      'invalid_bridge_contract',
      'Bridge contract metadata omitted bridge_version.',
      contract
    );
  }
  if (
    contract.bridge_source_sha !== null &&
    (typeof contract.bridge_source_sha !== 'string' ||
      !contract.bridge_source_sha.trim())
  ) {
    throw createContractError(
      'invalid_bridge_contract',
      'Bridge contract bridge_source_sha must be a non-empty string or null.',
      contract
    );
  }
  return contract;
}

export function createBridgeSessionClient({
  baseUrl,
  eventUrl,
  controlToken = null,
  fetchImpl = typeof fetch === 'function' ? fetch.bind(globalThis) : null,
  WebSocketImpl = typeof WebSocket === 'function' ? WebSocket : null
} = {}) {
  if (!baseUrl) throw new Error('Bridge session baseUrl is required');
  if (typeof fetchImpl !== 'function') throw new Error('fetch is required for bridge session mode');

  const base = resolveUrl(
    baseUrl,
    typeof window !== 'undefined' ? window.location.href : undefined
  );
  if (!base) throw new Error('Invalid bridge session baseUrl');

  const resolvedEventUrl =
    resolveUrl(eventUrl, typeof window !== 'undefined' ? window.location.href : undefined) ?? null;

  let socket = null;
  let socketClosed = false;
  let buffer = '';
  let negotiatedContract = null;
  let contractPromise = null;
  const decoder = typeof TextDecoder === 'function' ? new TextDecoder() : null;

  async function requestJson(path, { method = 'GET', body } = {}) {
    const response = await fetchImpl(resolveUrl(path, base), {
      method,
      headers:
        body === undefined && !controlToken
          ? undefined
          : {
              ...(body === undefined ? {} : { 'content-type': 'application/json' }),
              ...(controlToken ? { authorization: `Bearer ${controlToken}` } : {})
            },
      body: body === undefined ? undefined : JSON.stringify(body)
    });

    const text = await response.text();
    let payload = null;
    if (text) {
      try {
        payload = JSON.parse(text);
      } catch (_) {
        payload = null;
      }
    }

    if (!response.ok) {
      throw createBridgeError(payload, `Bridge request failed: ${response.status}`);
    }
    return payload ?? {};
  }

  async function getContract({ force = false } = {}) {
    if (!force && negotiatedContract) return negotiatedContract;
    if (!force && contractPromise) return contractPromise;
    contractPromise = requestJson('/api/contract')
      .then((payload) => {
        negotiatedContract = validateBridgeContract(payload.contract);
        return negotiatedContract;
      })
      .finally(() => {
        contractPromise = null;
      });
    return contractPromise;
  }

  async function getSession({ warm = false } = {}) {
    await getContract();
    const suffix = warm ? '?warm=1' : '';
    const payload = await requestJson(`/api/device/session${suffix}`);
    return payload.session ?? null;
  }

  async function stageConfig(config, {
    expectedSessionRevision,
    clientApplyId,
    stagedRevision,
    stagedDigest
  } = {}) {
    await getContract();
    const payload = await requestJson('/api/device/stage', {
      method: 'POST',
      body: {
        config,
        expectedSessionRevision,
        clientApplyId,
        stagedRevision,
        stagedDigest
      }
    });
    return payload.result ?? null;
  }

  async function applyConfig(body = {}) {
    await getContract();
    const payload = await requestJson('/api/device/apply', {
      method: 'POST',
      body
    });
    const session = payload.state?.deviceSession ?? null;
    if (!isRecognizedApplyResult(payload.result)) {
      throw createInvalidApplyResponseError(session);
    }
    return {
      result: payload.result,
      session
    };
  }

  async function rollbackConfig(reason = 'operator_request') {
    await getContract();
    const payload = await requestJson('/api/device/rollback', {
      method: 'POST',
      body: { reason }
    });
    return payload.result ?? null;
  }

  async function publishDisplayMetadata(metadata = {}) {
    await getContract();
    const payload = await requestJson('/api/display-metadata', {
      method: 'POST',
      body: metadata
    });
    return payload.metadata ?? null;
  }

  function closeEvents() {
    socketClosed = true;
    if (!socket) return;
    if (socket.readyState === WebSocketImpl.CLOSING || socket.readyState === WebSocketImpl.CLOSED) {
      socket = null;
      return;
    }
    socket.close();
    socket = null;
  }

  async function openEvents({ onEvent, onClose, onError } = {}) {
    const contract = await getContract();
    if (!resolvedEventUrl) {
      return Promise.reject(new Error('Bridge eventUrl is required'));
    }
    if (typeof WebSocketImpl !== 'function') {
      return Promise.reject(new Error('WebSocket unsupported'));
    }
    if (socket && socket.readyState <= 1) {
      return Promise.resolve();
    }

    socketClosed = false;
    buffer = '';
    const socketUrl = new URL(resolvedEventUrl);
    if (controlToken) socketUrl.searchParams.set('token', controlToken);
    socket = new WebSocketImpl(socketUrl.toString());
    socket.binaryType = 'arraybuffer';

    const deliverEvent = (message) => {
      if (message?.version !== contract.event_contract_version) {
        onError?.(
          createContractError(
            'unsupported_bridge_event_contract_version',
            `Bridge event used unsupported contract version: ${String(message?.version)}`,
            contract
          )
        );
        return;
      }
      onEvent?.(message);
    };

    const flushBufferedEvent = () => {
      const trimmed = buffer.trim();
      buffer = '';
      if (!trimmed) return;
      try {
        deliverEvent(JSON.parse(trimmed));
      } catch (err) {
        onError?.(err);
      }
    };

    const handleMessage = (event) => {
      const data = event?.data;
      const text = typeof data === 'string' ? data : decoder?.decode(data) ?? '';
      if (!text) return;
      buffer += text;
      let index;
      while ((index = buffer.indexOf('\n')) >= 0) {
        const segment = buffer.slice(0, index).trim();
        buffer = buffer.slice(index + 1);
        if (!segment) continue;
        try {
          deliverEvent(JSON.parse(segment));
        } catch (err) {
          onError?.(err);
        }
      }
    };

    const handleClose = () => {
      if (socketClosed) return;
      socketClosed = true;
      flushBufferedEvent();
      onClose?.();
    };

    return new Promise((resolve, reject) => {
      const cleanup = () => {
        socket?.removeEventListener?.('open', handleOpen);
        socket?.removeEventListener?.('error', handleOpenError);
      };

      const handleOpen = () => {
        cleanup();
        socket?.addEventListener?.('message', handleMessage);
        socket?.addEventListener?.('close', handleClose);
        resolve();
      };

      const handleOpenError = () => {
        cleanup();
        reject(new Error('Bridge event socket failed to open'));
      };

      socket.addEventListener('open', handleOpen, { once: true });
      socket.addEventListener('error', handleOpenError, { once: true });
      socket.addEventListener('error', (event) => {
        onError?.(event instanceof Error ? event : new Error('Bridge event socket error'));
      });
    });
  }

  return {
    getContract,
    getSession,
    stageConfig,
    applyConfig,
    rollbackConfig,
    publishDisplayMetadata,
    openEvents,
    closeEvents
  };
}
