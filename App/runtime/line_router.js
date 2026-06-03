import { handleNativePendingResponse } from './native_response_router.js';

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
