import { handleNativePendingResponse } from './native_response_router.js';

function nativeTextErrorForPending(line, activePending) {
  if (activePending?.protocolMode !== 'native' || !activePending.nativeRequest) return null;
  const text = String(line ?? '').trim();
  if (!text) return null;
  if (text.includes('Unknown command: SET_PROFILE_CHUNK')) {
    return new Error(
      'Firmware does not support chunked profile saves. Flash the latest firmware, then reconnect.'
    );
  }
  if (text.includes('Error: Command too long')) {
    return new Error(
      'Profile save command exceeded the firmware serial buffer. Refresh the App and flash the latest firmware.'
    );
  }
  if (text.startsWith('Unknown command:') || text.startsWith('Error:')) {
    return new Error(text);
  }
  return null;
}

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
      const activePending = rpcKernel.getActivePendingRpc();
      const textError = nativeTextErrorForPending(line, activePending);
      if (textError) {
        rpcKernel.handleRpcResponse({
          id: activePending.id,
          error: { message: textError.message }
        });
        return;
      }
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
    if (
      handleNativePendingResponse({
        msg,
        activePending,
        activePendingId,
        rpcKernel,
        isManifestPayload,
        isConfigPayload
      })
    ) {
      return;
    }
    if (msg.type === 'telemetry' || msg.slots || msg.envelopes || msg.lfos) {
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
