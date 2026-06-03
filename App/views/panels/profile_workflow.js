const DEFAULT_PROFILE_RPC_TIMEOUT_MS = 30000;

export function resolveProfileCapabilities(manifest) {
  const caps =
    manifest?.capabilities && typeof manifest.capabilities === 'object'
      ? manifest.capabilities
      : {};
  const hasExplicitCapabilities = Object.keys(caps).length > 0;
  const coreProfileFallback = manifest && !hasExplicitCapabilities;
  return {
    profileSave: hasExplicitCapabilities
      ? Boolean(caps.profile_save)
      : Boolean(coreProfileFallback),
    profileLoad: hasExplicitCapabilities
      ? Boolean(caps.profile_load)
      : Boolean(coreProfileFallback),
    profileReset: hasExplicitCapabilities
      ? Boolean(caps.profile_reset)
      : Boolean(coreProfileFallback),
    macroSnapshot: Boolean(caps.macro_snapshot),
    arpLive: Boolean(caps.arp_live),
    scenes: Boolean(caps.scenes)
  };
}

export function supportsAnyProfileAction(capabilities = {}) {
  return Boolean(capabilities.profileSave || capabilities.profileLoad || capabilities.profileReset);
}

export function supportsGuidedProfileFlow(capabilities = {}) {
  return Boolean(capabilities.profileSave && capabilities.profileLoad);
}

export function unsupportedProfileActionCopy(method) {
  switch (method) {
    case 'save_profile':
      return 'This firmware cannot archive the current deck state into slots A-D from the browser yet. Use Download profile for a file backup.';
    case 'load_profile':
      return 'This firmware cannot switch EEPROM profile slots from the browser yet. Use the device controls, then reconnect to inspect the active state.';
    case 'reset_profile':
      return 'This firmware cannot wipe a device profile from the browser yet. Load a baseline backup file instead.';
    default:
      return 'This firmware does not expose that profile action to the browser yet.';
  }
}

export function supportsProfileMethod(method, capabilities = {}) {
  switch (method) {
    case 'save_profile':
      return Boolean(capabilities.profileSave);
    case 'load_profile':
      return Boolean(capabilities.profileLoad);
    case 'reset_profile':
      return Boolean(capabilities.profileReset);
    default:
      return false;
  }
}

export function createProfileWorkflow({
  runtime,
  formRenderer,
  setStatus,
  clampSlot,
  slotCount = 4,
  getCapabilities = () => ({}),
  isInteractable = () => false,
  getActiveProfileSlot = () => 0,
  setActiveProfileSlot = () => {},
  describeSlot = () => 'Slot A',
  refreshProfileControls = () => {},
  refreshProfileUtilities = () => {},
  timeoutMs = DEFAULT_PROFILE_RPC_TIMEOUT_MS
} = {}) {
  let locked = false;

  function isLocked() {
    return locked;
  }

  function setLocked(nextLocked) {
    locked = Boolean(nextLocked);
  }

  async function runProfileRpc(
    method,
    { busyLabel, successLabel, successCopy, expectConfig } = {}
  ) {
    const capabilities = getCapabilities();
    if (!supportsProfileMethod(method, capabilities)) {
      setStatus('warn', 'Profile action unavailable', unsupportedProfileActionCopy(method));
      return { ok: false, reason: 'unsupported' };
    }
    if (!isInteractable()) {
      setStatus('warn', 'Profile offline', 'Connect to the deck before using profiles.');
      return { ok: false, reason: 'offline' };
    }

    const dirtyBefore = Boolean(runtime.getState().dirty);
    setLocked(true);
    refreshProfileControls();

    try {
      if (expectConfig) {
        formRenderer?.clearPendingPatches?.();
      }
      if (method === 'save_profile' && dirtyBefore) {
        setStatus('warn', 'Applying staged edits…', `${describeSlot()} • syncing before save`);
        await runtime.apply();
      }
    } catch (err) {
      setLocked(false);
      refreshProfileControls();
      setStatus('err', 'Apply failed', err.message || String(err));
      return { ok: false, reason: 'apply_failed', error: err };
    }

    setStatus('warn', busyLabel, `${describeSlot()} • busy`);
    let utilityRefreshNeeded = false;
    try {
      const response = await runtime.sendRpc(
        { rpc: method, slot: getActiveProfileSlot() },
        { timeoutMs }
      );
      const responseSlot = clampSlot(
        response?.slot ?? response?.profile ?? getActiveProfileSlot(),
        slotCount
      );
      setActiveProfileSlot(responseSlot);

      if (expectConfig) {
        let payload = response?.config ?? null;
        if (!payload || typeof payload !== 'object' || !Array.isArray(payload?.slots)) {
          const configPayload = await runtime.sendRpc({ rpc: 'get_config' }, { timeoutMs });
          payload = configPayload?.config ?? configPayload;
        }
        if (payload && typeof payload === 'object') {
          runtime.replaceConfig(payload);
        }
      }

      utilityRefreshNeeded = true;
      setStatus('ok', successLabel, successCopy ?? describeSlot());
      return { ok: true, response };
    } catch (err) {
      setStatus('err', `${successLabel ?? 'Profile'} failed`, err.message || String(err));
      return { ok: false, reason: 'rpc_failed', error: err };
    } finally {
      setLocked(false);
      refreshProfileControls();
      if (utilityRefreshNeeded) {
        void refreshProfileUtilities({ silent: true });
      }
    }
  }

  function reset() {
    setLocked(false);
  }

  return {
    isLocked,
    reset,
    runProfileRpc,
    setLocked
  };
}
