export function createRuntimeLineHandler({
  emit,
  notifyStatus,
  rpcKernel,
  handleSceneLine,
  handleMacroLine,
  isManifestPayload,
  isConfigPayload,
  applyConfigPatch,
  extractSlotIndex,
  onTelemetry
} = {}) {
  return function handleLine(line) {
    // Dispatch order matters: scene/macro replies can look like normal JSON payloads, so route
    // their handlers first before generic RPC/telemetry parsing.
    if (!line) return;
    let msg;
    try {
      msg = JSON.parse(line);
    } catch (err) {
      emit('log', line);
      return;
    }
    if (handleSceneLine(msg)) return;
    if (handleMacroLine(msg)) return;
    if (msg?.id !== undefined) {
      rpcKernel.handleRpcResponse(msg);
      return;
    }
    const activePending = rpcKernel.getActivePendingRpc();
    const activePendingId = activePending?.id;
    if (activePending?.protocolMode === 'native' && activePending.nativeRequest) {
      if (msg.type === 'error') {
        rpcKernel.handleRpcResponse({
          id: activePendingId,
          error: {
            code: msg.code,
            message: msg.message ?? msg.code ?? 'Device error'
          }
        });
        return;
      }
      switch (activePending.nativeRequest.kind) {
        case 'hello':
          if (msg.hello !== undefined) {
            rpcKernel.handleRpcResponse({
              id: activePendingId,
              result: { message: String(msg.hello) }
            });
            return;
          }
          break;
        case 'manifest':
          if (isManifestPayload(msg)) {
            rpcKernel.handleRpcResponse({ id: activePendingId, result: { manifest: msg } });
            return;
          }
          break;
        case 'config':
          if (isConfigPayload(msg)) {
            rpcKernel.handleRpcResponse({ id: activePendingId, result: { config: msg } });
            return;
          }
          break;
        case 'schema':
          if (msg.$schema || msg.type === 'object' || msg.properties) {
            rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
            return;
          }
          break;
        case 'ack':
          if (msg.type === 'ack') {
            rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
            return;
          }
          break;
        case 'enter_config_mode':
          if (msg.type === 'response' && msg.command === 'ENTER_CONFIG_MODE') {
            if (msg.status === 'ok') {
              rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
            } else {
              rpcKernel.handleRpcResponse({
                id: activePendingId,
                error: { message: msg.message ?? 'Config boot request failed' }
              });
            }
            return;
          }
          break;
        case 'profile_save':
          if (Object.prototype.hasOwnProperty.call(msg, 'profile_saved')) {
            if (msg.profile_saved) {
              rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
            } else {
              rpcKernel.handleRpcResponse({
                id: activePendingId,
                error: { message: msg.error ?? 'Profile save failed' }
              });
            }
            return;
          }
          break;
        case 'profile_load':
          if (Object.prototype.hasOwnProperty.call(msg, 'profile_loaded')) {
            if (msg.profile_loaded) {
              rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
            } else {
              rpcKernel.handleRpcResponse({
                id: activePendingId,
                error: { message: msg.error ?? 'Profile load failed' }
              });
            }
            return;
          }
          break;
        case 'profile_reset':
          if (Object.prototype.hasOwnProperty.call(msg, 'profile_reset')) {
            if (msg.profile_reset) {
              rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
            } else {
              rpcKernel.handleRpcResponse({
                id: activePendingId,
                error: { message: msg.error ?? 'Profile reset failed' }
              });
            }
            return;
          }
          break;
        default:
          break;
      }
    }
    if (msg.type === 'telemetry' || msg.slots || msg.envelopes) {
      onTelemetry(msg);
      return;
    }
    if (msg.type === 'config-patch') {
      applyConfigPatch(msg);
      return;
    }
    if (msg.type === 'slot_patch' && msg.slot && typeof msg.slot === 'object') {
      const slotBody = { ...msg.slot };
      const index = extractSlotIndex(slotBody, msg.slot_index ?? msg.index ?? msg.id);
      if (index !== null) {
        slotBody.index = index;
        applyConfigPatch({ slots: [slotBody] });
      }
      return;
    }
    if (msg.type === 'error') {
      emit('device-error', msg);
      return;
    }
    if (msg.status || msg.event || msg.type === 'status') {
      notifyStatus(msg);
      return;
    }
    emit('log', line);
  };
}
