import {
  clampProfileSlot,
  persistProfileSlot,
  readProfileSlotPreference
} from '../state/ui_preferences.js';
import { createProfileFileIO } from './profile_file_io.js';

const PROFILE_LABELS = ['A', 'B', 'C', 'D'];
const MACRO_SAVE_COMMAND = 'SAVE_MACRO_SLOT';
const MACRO_RECALL_COMMAND = 'RECALL_MACRO_SLOT';
const SCENE_SLOT_COUNT = 6;

export function createProfileMacroScenePanel({
  runtime,
  formRenderer,
  localManifest,
  setStatus,
  elements = {}
} = {}) {
  const {
    profileSlotButtons = [],
    profileSlotStatus = null,
    profileSaveBtn = null,
    profileLoadBtn = null,
    profileResetBtn = null,
    profileWizardTarget = null,
    profileWizardSwitchBtn = null,
    profileWizardApplyBtn = null,
    profileWizardSaveBtn = null,
    profileWizardStatus = null,
    profileDownloadBtn = null,
    profileUploadBtn = null,
    profileHint = null,
    macroSaveBtn = null,
    macroRecallBtn = null,
    macroStatusEl = null,
    sceneGrid = null,
    sceneStatusEl = null
  } = elements;

  let profileInteractable = false;
  let profileRpcLocked = false;
  let profileWizardBusy = false;
  let macroBusy = false;
  let macroAvailable = false;
  let sceneBusy = false;
  let activeProfileSlot = readProfileSlotPreference({ slotCount: PROFILE_LABELS.length });
  let profileWizardTargetSlot = activeProfileSlot;
  let deviceCapabilities = resolveCapabilities(localManifest);
  const sceneSlotState = Array.from({ length: SCENE_SLOT_COUNT }, () => ({
    name: '',
    available: false
  }));
  const sceneSlotElements = [];
  let bound = false;
  const profileFileIO = createProfileFileIO({
    runtime,
    setStatus,
    clampSlot: clampProfileSlot,
    slotCount: PROFILE_LABELS.length,
    getActiveProfileSlot: () => activeProfileSlot,
    slotLabel,
    describeSlot,
    setActiveProfileSlot
  });

  // Collapse manifest capability flags into a simpler UI-facing shape.
  function resolveCapabilities(manifest) {
    const caps =
      manifest?.capabilities && typeof manifest.capabilities === 'object'
        ? manifest.capabilities
        : {};
    return {
      profileSave: Boolean(caps.profile_save),
      profileLoad: Boolean(caps.profile_load),
      profileReset: Boolean(caps.profile_reset),
      macroSnapshot: Boolean(caps.macro_snapshot),
      scenes: Boolean(caps.scenes)
    };
  }

  // Some UI paths only need to know whether any device-backed profile action exists.
  function supportsAnyProfileAction() {
    return (
      deviceCapabilities.profileSave ||
      deviceCapabilities.profileLoad ||
      deviceCapabilities.profileReset
    );
  }

  // Guided save/switch flows only make sense when both load and save are exposed.
  function supportsGuidedProfileFlow() {
    return deviceCapabilities.profileSave && deviceCapabilities.profileLoad;
  }

  // Explain whether the active profile slot reflects real device storage or only
  // the browser's chosen target.
  function profileSlotModeCopy() {
    return supportsAnyProfileAction() ? 'mirrored' : 'local target';
  }

  // Keep the recovery/profile hint text honest as capabilities and connectivity change.
  function updateProfileHint() {
    if (!profileHint) return;
    if (!profileInteractable) {
      profileHint.textContent =
        'Download/upload always works as a file backup. Connect to see whether this firmware exposes device-backed profile actions.';
      return;
    }
    if (supportsAnyProfileAction()) {
      profileHint.textContent =
        'Device-backed profile actions are enabled for this firmware. Download/upload remains the safest file backup.';
      return;
    }
    profileHint.textContent =
      'This firmware does not expose browser-driven profile save, switch, or reset yet. Use Download/Upload for file backups.';
  }

  // Return operator-facing copy for an unsupported profile RPC.
  function unsupportedProfileActionCopy(method) {
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

  // Gate a specific browser-triggered profile method on manifest support.
  function supportsProfileMethod(method) {
    switch (method) {
      case 'save_profile':
        return deviceCapabilities.profileSave;
      case 'load_profile':
        return deviceCapabilities.profileLoad;
      case 'reset_profile':
        return deviceCapabilities.profileReset;
      default:
        return false;
    }
  }

  // Synchronize the macro/scene/profile helper text with the current capability set.
  function syncRecoverySupportCopy() {
    updateProfileHint();
    if (!profileInteractable) {
      setMacroStatus('muted', 'Connect to see whether macro storage is available.');
      setSceneStatus('muted', 'Connect to see whether scene storage is available.');
      return;
    }
    if (!deviceCapabilities.macroSnapshot) {
      setMacroStatus('muted', 'Macro snapshot storage is unavailable on this firmware.');
    } else if (!macroBusy && !macroAvailable) {
      setMacroStatus('muted', 'No macro snapshot stored yet.');
    }
    if (!deviceCapabilities.scenes) {
      setSceneStatus('muted', 'Scene storage is unavailable on this firmware.');
    }
  }

  // Translate a slot index into the A-D label shown in the profile controls.
  function slotLabel(index) {
    return PROFILE_LABELS[index] ?? PROFILE_LABELS[0];
  }

  // User-facing description for the currently targeted profile slot.
  function describeSlot(index = activeProfileSlot) {
    return `Slot ${slotLabel(index)}`;
  }

  // Update the selected profile slot and mirror it into the UI/state badges.
  function setActiveProfileSlot(index, { persist = true } = {}) {
    const bounded = clampProfileSlot(index, PROFILE_LABELS.length);
    activeProfileSlot = bounded;
    profileSlotButtons.forEach((button) => {
      const slotValue = Number(button.dataset.profileSlot);
      if (!Number.isFinite(slotValue)) return;
      button.setAttribute('aria-pressed', slotValue === bounded ? 'true' : 'false');
    });
    if (profileSlotStatus) {
      profileSlotStatus.textContent = `${describeSlot(bounded)} • ${profileSlotModeCopy()}`;
    }
    if (persist) {
      persistProfileSlot(bounded, { slotCount: PROFILE_LABELS.length });
    }
  }

  // Recompute enabled/disabled state for all profile/macro/scene controls.
  function refreshProfileControls() {
    const canInteract = profileInteractable && !profileRpcLocked;
    if (profileSaveBtn) profileSaveBtn.disabled = !canInteract || !deviceCapabilities.profileSave;
    if (profileLoadBtn) profileLoadBtn.disabled = !canInteract || !deviceCapabilities.profileLoad;
    if (profileResetBtn)
      profileResetBtn.disabled = !canInteract || !deviceCapabilities.profileReset;
    updateProfileWizardControls();
    updateMacroControls();
    updateSceneControls();
    updateProfileHint();
  }

  // Single sink for the guided profile wizard status line.
  function setProfileWizardStatus(state, message) {
    if (!profileWizardStatus) return;
    profileWizardStatus.dataset.state = state;
    profileWizardStatus.textContent = message;
  }

  // Drive the three-step profile wizard copy and button gating from current state.
  function updateProfileWizardControls() {
    // Guided flow state machine: switch to target slot -> apply dirty edits -> save slot.
    const target = clampProfileSlot(
      Number(profileWizardTarget?.value ?? profileWizardTargetSlot),
      PROFILE_LABELS.length
    );
    profileWizardTargetSlot = target;
    const guidedSupported = supportsGuidedProfileFlow();
    const canInteract =
      profileInteractable && !profileRpcLocked && !profileWizardBusy && guidedSupported;
    if (profileWizardTarget) profileWizardTarget.disabled = !canInteract;
    if (profileWizardSwitchBtn) profileWizardSwitchBtn.disabled = !canInteract;
    const dirtyNow = runtime.getState().dirty;
    const onTarget = activeProfileSlot === target;
    if (profileWizardApplyBtn) {
      profileWizardApplyBtn.disabled = !canInteract || !onTarget || !dirtyNow;
    }
    if (profileWizardSaveBtn) {
      profileWizardSaveBtn.disabled = !canInteract || !onTarget;
    }

    if (!profileInteractable) {
      setProfileWizardStatus('muted', 'Connect to the device to start the guided flow.');
      return;
    }
    if (!guidedSupported) {
      setProfileWizardStatus(
        'muted',
        'This firmware does not expose browser-driven profile switch/save yet. Use Download/Upload for file backups.'
      );
      return;
    }
    if (!onTarget) {
      setProfileWizardStatus(
        'busy',
        `Step 1: switch to Slot ${slotLabel(target)} (currently ${slotLabel(activeProfileSlot)}).`
      );
      return;
    }
    if (dirtyNow) {
      setProfileWizardStatus('busy', `Step 2: apply edits, then save Slot ${slotLabel(target)}.`);
      return;
    }
    setProfileWizardStatus(
      'ok',
      `Step 3: save Slot ${slotLabel(target)} to store current mapping.`
    );
  }

  // Gate macro buttons on capability, connectivity, and pending RPC state.
  function updateMacroControls() {
    const offline = !profileInteractable;
    const unsupported = !deviceCapabilities.macroSnapshot;
    if (macroSaveBtn) macroSaveBtn.disabled = offline || macroBusy || unsupported;
    if (macroRecallBtn) {
      macroRecallBtn.disabled = offline || macroBusy || unsupported || !macroAvailable;
    }
  }

  // Update the macro status copy line.
  function setMacroStatus(state, message) {
    if (!macroStatusEl) return;
    macroStatusEl.dataset.state = state;
    if (typeof message === 'string') {
      macroStatusEl.textContent = message;
    }
  }

  // Update the scene status copy line.
  function setSceneStatus(state, message) {
    if (!sceneStatusEl) return;
    sceneStatusEl.dataset.state = state;
    if (typeof message === 'string') {
      sceneStatusEl.textContent = message;
    }
  }

  // Mirror one scene slot's saved/empty state into the UI.
  function updateSceneSlot(slotIndex, { name, available }) {
    const slotInfo = sceneSlotElements[slotIndex];
    if (!slotInfo) return;
    const displayName = available
      ? name || `Scene ${slotIndex + 1}`
      : `Slot ${slotIndex + 1} empty`;
    if (slotInfo.statusEl) slotInfo.statusEl.textContent = displayName;
    sceneSlotState[slotIndex] = { name: name ?? '', available: Boolean(available) };
    updateSceneControls();
  }

  // Recompute enabled/disabled state for all scene save/recall buttons.
  function updateSceneControls() {
    const offline = !profileInteractable;
    const unsupported = !deviceCapabilities.scenes;
    sceneSlotElements.forEach((slotInfo) => {
      const state = sceneSlotState[slotInfo.slot];
      if (slotInfo.saveBtn) slotInfo.saveBtn.disabled = offline || sceneBusy || unsupported;
      if (slotInfo.recallBtn) {
        slotInfo.recallBtn.disabled = offline || sceneBusy || unsupported || !state.available;
      }
    });
  }

  // Ask the runtime for the latest scene directory so the UI reflects device truth.
  async function refreshSceneList() {
    if (!sceneGrid) return;
    if (!deviceCapabilities.scenes) {
      setSceneStatus('muted', 'Scene storage is unavailable on this firmware.');
      return;
    }
    setSceneStatus('busy', 'Loading scenes…');
    try {
      await runtime.requestScenes();
      setSceneStatus('muted', 'Scenes synced with the deck.');
    } catch (err) {
      setSceneStatus('err', `Scenes refresh failed: ${err.message || String(err)}`);
    }
  }

  // Save the current deck state into one named scene slot.
  async function handleSceneSave(slotIndex) {
    if (!deviceCapabilities.scenes) {
      setSceneStatus('muted', 'Scene storage is unavailable on this firmware.');
      setStatus(
        'warn',
        'Scenes unavailable',
        'This firmware does not expose scene storage to the browser.'
      );
      return;
    }
    if (!profileInteractable || sceneBusy) return;
    const slotInfo = sceneSlotElements[slotIndex];
    if (!slotInfo) return;
    const name = slotInfo.nameInput?.value.trim();
    sceneBusy = true;
    updateSceneControls();
    setSceneStatus('busy', `Saving scene ${slotIndex + 1}…`);
    try {
      await runtime.sendSceneCommand({
        cmd: 'SAVE_SCENE',
        slot: slotIndex,
        name: name || undefined
      });
      setSceneStatus('ok', `Scene ${slotIndex + 1} saved.`);
      await refreshSceneList();
    } catch (err) {
      setSceneStatus('err', `Scene save failed: ${err.message || String(err)}`);
    } finally {
      sceneBusy = false;
      updateSceneControls();
    }
  }

  // Recall one saved scene back into the live deck state.
  async function handleSceneRecall(slotIndex) {
    if (!deviceCapabilities.scenes) {
      setSceneStatus('muted', 'Scene storage is unavailable on this firmware.');
      setStatus(
        'warn',
        'Scenes unavailable',
        'This firmware does not expose scene storage to the browser.'
      );
      return;
    }
    if (!profileInteractable || sceneBusy) return;
    sceneBusy = true;
    updateSceneControls();
    setSceneStatus('busy', `Recalling scene ${slotIndex + 1}…`);
    try {
      await runtime.sendSceneCommand({ cmd: 'RECALL_SCENE', slot: slotIndex });
      setSceneStatus('ok', `Scene ${slotIndex + 1} recalled.`);
      try {
        const configPayload = await runtime.sendRpc({ rpc: 'get_config' });
        const configData = configPayload?.config ?? configPayload;
        if (configData && typeof configData === 'object') {
          runtime.replaceConfig(configData);
        }
      } catch (refreshErr) {
        setStatus('warn', 'Config refresh failed', refreshErr.message || String(refreshErr));
      }
      await refreshSceneList();
    } catch (err) {
      setSceneStatus('err', `Scene recall failed: ${err.message || String(err)}`);
    } finally {
      sceneBusy = false;
      updateSceneControls();
    }
  }

  async function runMacroCommand(command) {
    if (!deviceCapabilities.macroSnapshot) {
      setMacroStatus('muted', 'Macro snapshot storage is unavailable on this firmware.');
      setStatus(
        'warn',
        'Macro unavailable',
        'This firmware does not expose macro snapshot storage to the browser.'
      );
      return;
    }
    if (!profileInteractable) {
      setMacroStatus('muted', 'Connect to the deck before using macro snapshots.');
      setStatus('warn', 'Macro offline', 'Connect to the deck before using macro snapshots.');
      return;
    }
    macroBusy = true;
    updateMacroControls();
    const isSave = command === MACRO_SAVE_COMMAND;
    setMacroStatus('busy', isSave ? 'Saving macro snapshot…' : 'Recalling macro snapshot…');
    try {
      await runtime.sendMacroCommand(command);
      setMacroStatus(
        'ok',
        isSave
          ? 'Macro snapshot stored in EEPROM slot 254.'
          : 'Macro snapshot recalled from EEPROM slot 254.'
      );
      if (!isSave) {
        try {
          const configPayload = await runtime.sendRpc({ rpc: 'get_config' });
          const configData = configPayload?.config ?? configPayload;
          if (configData && typeof configData === 'object') {
            runtime.replaceConfig(configData);
          }
        } catch (refreshErr) {
          setStatus('warn', 'Config refresh failed', refreshErr.message || String(refreshErr));
        }
      }
    } catch (err) {
      const label = isSave ? 'Macro save' : 'Macro recall';
      setMacroStatus('err', `${label} failed: ${err.message || String(err)}`);
      setStatus('err', `${label} failed`, err.message || String(err));
    } finally {
      macroBusy = false;
      updateMacroControls();
    }
  }

  async function runProfileRpc(
    method,
    { busyLabel, successLabel, successCopy, expectConfig } = {}
  ) {
    // Shared profile RPC lane: one in-flight action at a time to keep slot/apply state coherent.
    if (!supportsProfileMethod(method)) {
      setStatus('warn', 'Profile action unavailable', unsupportedProfileActionCopy(method));
      return;
    }
    if (!profileInteractable) {
      setStatus('warn', 'Profile offline', 'Connect to the deck before using profiles.');
      return;
    }
    const dirtyBefore = runtime.getState().dirty;
    profileRpcLocked = true;
    refreshProfileControls();
    try {
      if (expectConfig) {
        // Loading/resetting a profile replaces the staged snapshot, so drop any debounced
        // field-level patches before they replay stale edits back into the fresh config.
        formRenderer.clearPendingPatches();
      }
      // Saving a profile always snapshots firmware state, so stage/apply first to avoid storing
      // stale EEPROM values when local edits are still dirty.
      if (method === 'save_profile' && dirtyBefore) {
        setStatus('warn', 'Applying staged edits…', `${describeSlot()} • syncing before save`);
        await runtime.apply();
      }
    } catch (err) {
      profileRpcLocked = false;
      refreshProfileControls();
      setStatus('err', 'Apply failed', err.message || String(err));
      return;
    }
    setStatus('warn', busyLabel, `${describeSlot()} • busy`);
    try {
      const response = await runtime.sendRpc({ rpc: method, slot: activeProfileSlot });
      // Firmware may report an authoritative slot index; trust it and realign local selection.
      const responseSlot = clampProfileSlot(
        response?.slot ?? response?.profile ?? activeProfileSlot,
        PROFILE_LABELS.length
      );
      setActiveProfileSlot(responseSlot);
      if (expectConfig) {
        let payload = response?.config ?? null;
        if (!payload || typeof payload !== 'object' || !Array.isArray(payload?.slots)) {
          const configPayload = await runtime.sendRpc({ rpc: 'get_config' });
          payload = configPayload?.config ?? configPayload;
        }
        if (payload && typeof payload === 'object') {
          runtime.replaceConfig(payload);
        }
      }
      setStatus('ok', successLabel, successCopy ?? describeSlot());
    } catch (err) {
      setStatus('err', `${successLabel ?? 'Profile'} failed`, err.message || String(err));
    } finally {
      profileRpcLocked = false;
      refreshProfileControls();
    }
  }

  async function handleWizardSwitchProfile() {
    // Step 1: bind the UI to the selected slot and pull that slot's config into staged/live state.
    if (!profileInteractable || profileRpcLocked || profileWizardBusy) return;
    profileWizardBusy = true;
    refreshProfileControls();
    try {
      setActiveProfileSlot(profileWizardTargetSlot);
      await runProfileRpc('load_profile', {
        busyLabel: 'Switching profile…',
        successLabel: 'Profile switched',
        successCopy: `${describeSlot()} active`,
        expectConfig: true
      });
    } finally {
      profileWizardBusy = false;
      refreshProfileControls();
    }
  }

  async function handleWizardApplyEdits() {
    // Step 2: commit staged edits to firmware (without persisting profile slot yet).
    if (!profileInteractable || profileRpcLocked || profileWizardBusy) return;
    if (activeProfileSlot !== profileWizardTargetSlot) {
      setStatus(
        'warn',
        'Wrong slot',
        `Switch to Slot ${slotLabel(profileWizardTargetSlot)} first.`
      );
      return;
    }
    if (!runtime.getState().dirty) {
      setStatus('warn', 'Nothing to apply', 'Adjust a parameter before applying.');
      return;
    }
    profileWizardBusy = true;
    refreshProfileControls();
    try {
      setStatus('warn', 'Applying…', `${describeSlot()} • waiting for firmware ACK`);
      await runtime.apply();
      setStatus('ok', 'Synced', `${describeSlot()} applied. Now save to persist.`);
    } catch (err) {
      setStatus('err', 'Apply failed', err.message || String(err));
    } finally {
      profileWizardBusy = false;
      refreshProfileControls();
    }
  }

  async function handleWizardSaveProfile() {
    // Step 3: persist the currently active slot after Step 2 succeeds.
    if (!profileInteractable || profileRpcLocked || profileWizardBusy) return;
    if (activeProfileSlot !== profileWizardTargetSlot) {
      setStatus(
        'warn',
        'Wrong slot',
        `Switch to Slot ${slotLabel(profileWizardTargetSlot)} first.`
      );
      return;
    }
    profileWizardBusy = true;
    refreshProfileControls();
    try {
      await runProfileRpc('save_profile', {
        busyLabel: 'Saving profile…',
        successLabel: 'Profile saved',
        successCopy: `${describeSlot()} archived`
      });
    } finally {
      profileWizardBusy = false;
      refreshProfileControls();
    }
  }

  function initializeSceneGrid() {
    if (!sceneGrid) return;
    for (let slotIndex = 0; slotIndex < SCENE_SLOT_COUNT; slotIndex++) {
      const slotEl = document.createElement('div');
      slotEl.className = 'scene-slot';
      slotEl.dataset.sceneSlot = String(slotIndex);
      slotEl.innerHTML = `
        <div class="scene-slot-meta">
          <span>Scene ${slotIndex + 1}</span>
          <span class="scene-slot-status">Slot ${slotIndex + 1} empty</span>
        </div>
        <input class="scene-name-input" type="text" maxlength="15" placeholder="Name (optional)" />
        <div class="scene-actions">
          <button class="scene-save" type="button" title="Save current state to this slot.">Save</button>
          <button class="scene-recall" type="button" title="Recall this scene snapshot.">Recall</button>
        </div>`;
      sceneGrid.appendChild(slotEl);
      const slotInfo = {
        slot: slotIndex,
        element: slotEl,
        statusEl: slotEl.querySelector('.scene-slot-status'),
        nameInput: slotEl.querySelector('.scene-name-input'),
        saveBtn: slotEl.querySelector('.scene-save'),
        recallBtn: slotEl.querySelector('.scene-recall')
      };
      slotInfo.saveBtn?.addEventListener('click', () => handleSceneSave(slotIndex));
      slotInfo.recallBtn?.addEventListener('click', () => handleSceneRecall(slotIndex));
      sceneSlotElements.push(slotInfo);
    }
  }

  function bind() {
    if (bound) return;
    bound = true;
    initializeSceneGrid();

    profileSlotButtons.forEach((button) => {
      button.addEventListener('click', () => {
        const slotIndex = clampProfileSlot(
          Number(button.dataset.profileSlot),
          PROFILE_LABELS.length
        );
        setActiveProfileSlot(slotIndex);
      });
    });
    if (profileWizardTarget) {
      profileWizardTarget.value = String(profileWizardTargetSlot);
      profileWizardTarget.addEventListener('change', () => {
        profileWizardTargetSlot = clampProfileSlot(
          Number(profileWizardTarget.value),
          PROFILE_LABELS.length
        );
        updateProfileWizardControls();
      });
    }
    profileWizardSwitchBtn?.addEventListener('click', () => handleWizardSwitchProfile());
    profileWizardApplyBtn?.addEventListener('click', () => handleWizardApplyEdits());
    profileWizardSaveBtn?.addEventListener('click', () => handleWizardSaveProfile());
    setActiveProfileSlot(activeProfileSlot, { persist: false });
    refreshProfileControls();

    macroSaveBtn?.addEventListener('click', () => {
      const confirmation =
        typeof window !== 'undefined' && typeof window.confirm === 'function'
          ? window.confirm('Overwrite the macro snapshot stored in EEPROM slot 254?')
          : true;
      if (!confirmation) return;
      runMacroCommand(MACRO_SAVE_COMMAND);
    });
    macroRecallBtn?.addEventListener('click', () => runMacroCommand(MACRO_RECALL_COMMAND));
    setMacroStatus('muted', 'Awaiting the first snapshot.');

    profileSaveBtn?.addEventListener('click', () =>
      runProfileRpc('save_profile', {
        busyLabel: 'Saving profile…',
        successLabel: 'Profile saved',
        successCopy: `${describeSlot()} archived`
      })
    );
    profileLoadBtn?.addEventListener('click', () =>
      runProfileRpc('load_profile', {
        busyLabel: 'Switching profile…',
        successLabel: 'Profile switched',
        successCopy: `${describeSlot()} active`,
        expectConfig: true
      })
    );
    profileResetBtn?.addEventListener('click', () =>
      runProfileRpc('reset_profile', {
        busyLabel: 'Resetting profile…',
        successLabel: 'Profile reset',
        successCopy: `${describeSlot()} restored`,
        expectConfig: true
      })
    );
    profileDownloadBtn?.addEventListener('click', () => profileFileIO.handleProfileDownload());
    profileUploadBtn?.addEventListener('click', () => profileFileIO.handleProfileUpload());
  }

  function onConfigChanged() {
    updateProfileWizardControls();
  }

  function onManifest(manifest) {
    deviceCapabilities = resolveCapabilities(manifest);
    macroAvailable = deviceCapabilities.macroSnapshot ? macroAvailable : false;
    setActiveProfileSlot(activeProfileSlot, { persist: false });
    syncRecoverySupportCopy();
    refreshProfileControls();
  }

  function onConnected() {
    profileInteractable = true;
    refreshProfileControls();
    syncRecoverySupportCopy();
    if (deviceCapabilities.scenes) {
      refreshSceneList();
    }
  }

  function onDisconnected() {
    deviceCapabilities = resolveCapabilities(localManifest);
    profileInteractable = false;
    profileRpcLocked = false;
    macroAvailable = false;
    setActiveProfileSlot(activeProfileSlot, { persist: false });
    refreshProfileControls();
    syncRecoverySupportCopy();
  }

  function onRuntimeError() {
    deviceCapabilities = resolveCapabilities(localManifest);
    profileInteractable = false;
    profileRpcLocked = false;
    macroAvailable = false;
    setActiveProfileSlot(activeProfileSlot, { persist: false });
    refreshProfileControls();
    syncRecoverySupportCopy();
  }

  function onMacro({ available } = {}) {
    if (!deviceCapabilities.macroSnapshot) return;
    if (available === undefined) return;
    macroAvailable = Boolean(available);
    if (!macroAvailable && !macroBusy) {
      setMacroStatus('muted', 'No macro snapshot stored yet.');
    }
    updateMacroControls();
  }

  function onScene(payload) {
    if (!sceneSlotElements.length || !payload) return;
    if (payload.type === 'list' && Array.isArray(payload.scenes)) {
      payload.scenes.forEach((entry) => {
        if (typeof entry.slot === 'number') {
          updateSceneSlot(entry.slot, {
            name: entry.name ?? '',
            available: entry.available
          });
        }
      });
      setSceneStatus('muted', 'Scenes synced with the deck.');
      return;
    }
    if (payload.type === 'saved' && typeof payload.slot === 'number') {
      updateSceneSlot(payload.slot, {
        name: payload.name ?? '',
        available: payload.available
      });
    }
    if (payload.type === 'recalled' && typeof payload.slot === 'number') {
      updateSceneSlot(payload.slot, {
        name: payload.name ?? '',
        available: payload.available
      });
    }
  }

  return {
    bind,
    onConfigChanged,
    onManifest,
    onConnected,
    onDisconnected,
    onRuntimeError,
    onMacro,
    onScene
  };
}
