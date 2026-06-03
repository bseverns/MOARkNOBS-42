export function handleNativePendingResponse({
  msg,
  activePending,
  activePendingId,
  rpcKernel,
  isManifestPayload,
  isConfigPayload
} = {}) {
  if (activePending?.protocolMode !== 'native' || !activePending.nativeRequest) return false;

  if (msg.type === 'error') {
    rpcKernel.handleRpcResponse({
      id: activePendingId,
      error: {
        code: msg.code,
        message: msg.message ?? msg.code ?? 'Device error'
      }
    });
    return true;
  }

  switch (activePending.nativeRequest.kind) {
    case 'hello':
      if (msg.hello !== undefined) {
        rpcKernel.handleRpcResponse({
          id: activePendingId,
          result: { message: String(msg.hello) }
        });
        return true;
      }
      break;
    case 'manifest':
      if (isManifestPayload(msg)) {
        rpcKernel.handleRpcResponse({ id: activePendingId, result: { manifest: msg } });
        return true;
      }
      break;
    case 'mod_matrix':
      if (
        msg.command === 'GET_MOD_MATRIX' &&
        Array.isArray(msg.routes) &&
        msg.sources &&
        typeof msg.sources === 'object'
      ) {
        rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
        return true;
      }
      break;
    case 'profile_get':
      if (
        Object.prototype.hasOwnProperty.call(msg, 'profile') &&
        msg.arp &&
        typeof msg.arp === 'object'
      ) {
        rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
        return true;
      }
      break;
    case 'clock_get':
    case 'clock_set':
      if (
        Object.prototype.hasOwnProperty.call(msg, 'follow_external') &&
        Object.prototype.hasOwnProperty.call(msg, 'clock_out_enabled') &&
        Object.prototype.hasOwnProperty.call(msg, 'tapped_bpm')
      ) {
        if (activePending.nativeRequest.kind === 'clock_set' && msg.status !== 'ok') {
          rpcKernel.handleRpcResponse({
            id: activePendingId,
            error: { message: msg.message ?? 'Clock update failed' }
          });
        } else {
          rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
        }
        return true;
      }
      break;
    case 'arp_get':
    case 'arp_set':
      if (
        Object.prototype.hasOwnProperty.call(msg, 'length_ticks') &&
        Object.prototype.hasOwnProperty.call(msg, 'shape') &&
        Object.prototype.hasOwnProperty.call(msg, 'gate_percent') &&
        Object.prototype.hasOwnProperty.call(msg, 'octave_range')
      ) {
        if (activePending.nativeRequest.kind === 'arp_set' && msg.status !== 'ok') {
          rpcKernel.handleRpcResponse({
            id: activePendingId,
            error: { message: msg.message ?? 'Arp update failed' }
          });
        } else {
          rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
        }
        return true;
      }
      break;
    case 'jitter_get':
    case 'jitter_set':
      if (
        Object.prototype.hasOwnProperty.call(msg, 'depth') &&
        Object.prototype.hasOwnProperty.call(msg, 'smoothness')
      ) {
        if (activePending.nativeRequest.kind === 'jitter_set' && msg.status !== 'ok') {
          rpcKernel.handleRpcResponse({
            id: activePendingId,
            error: { message: msg.message ?? 'Jitter update failed' }
          });
        } else {
          rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
        }
        return true;
      }
      break;
    case 'note_dynamics_get':
    case 'note_dynamics_set':
      if (
        Object.prototype.hasOwnProperty.call(msg, 'velocity_shift') &&
        Object.prototype.hasOwnProperty.call(msg, 'change_probability')
      ) {
        if (activePending.nativeRequest.kind === 'note_dynamics_set' && msg.status !== 'ok') {
          rpcKernel.handleRpcResponse({
            id: activePendingId,
            error: { message: msg.message ?? 'Note dynamics update failed' }
          });
        } else {
          rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
        }
        return true;
      }
      break;
    case 'profile_set':
      if (msg.type === 'response' && msg.status) {
        if (msg.status === 'ok') {
          rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
        } else {
          rpcKernel.handleRpcResponse({
            id: activePendingId,
            error: { message: msg.message ?? 'Profile update failed' }
          });
        }
        return true;
      }
      break;
    case 'usb_midi_get':
      if (Object.prototype.hasOwnProperty.call(msg, 'usb_midi_out')) {
        rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
        return true;
      }
      break;
    case 'usb_midi_set':
      if (
        msg.command === 'SET_USB_MIDI' &&
        Object.prototype.hasOwnProperty.call(msg, 'usb_midi_out')
      ) {
        if (msg.status === 'ok') {
          rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
        } else {
          rpcKernel.handleRpcResponse({
            id: activePendingId,
            error: { message: msg.message ?? 'USB MIDI update failed' }
          });
        }
        return true;
      }
      break;
    case 'config':
      if (isConfigPayload(msg)) {
        rpcKernel.handleRpcResponse({ id: activePendingId, result: { config: msg } });
        return true;
      }
      break;
    case 'schema':
      if (msg.$schema || msg.type === 'object' || msg.properties) {
        rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
        return true;
      }
      break;
    case 'ack':
      if (msg.type === 'ack') {
        rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
        return true;
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
        return true;
      }
      break;
    case 'arp_start':
      if (Object.prototype.hasOwnProperty.call(msg, 'arp_started')) {
        if (msg.arp_started) {
          rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
        } else {
          rpcKernel.handleRpcResponse({
            id: activePendingId,
            error: { message: msg.error ?? 'Arp start failed' }
          });
        }
        return true;
      }
      break;
    case 'arp_stop':
      if (Object.prototype.hasOwnProperty.call(msg, 'arp_stopped')) {
        if (msg.arp_stopped) {
          rpcKernel.handleRpcResponse({ id: activePendingId, result: msg });
        } else {
          rpcKernel.handleRpcResponse({
            id: activePendingId,
            error: { message: msg.error ?? 'Arp stop failed' }
          });
        }
        return true;
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
        return true;
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
        return true;
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
        return true;
      }
      break;
    default:
      break;
  }

  return false;
}
