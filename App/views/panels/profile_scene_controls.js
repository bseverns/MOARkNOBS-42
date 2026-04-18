const DEFAULT_SCENE_SLOT_COUNT = 6;

export function createProfileSceneControls({
  runtime,
  setStatus,
  sceneGrid = null,
  sceneStatusEl = null,
  sceneSlotCount = DEFAULT_SCENE_SLOT_COUNT,
  isInteractable = () => false,
  supportsScenes = () => false
} = {}) {
  const sceneSlotState = Array.from({ length: sceneSlotCount }, () => ({
    name: '',
    available: false
  }));
  const sceneSlotElements = [];
  let sceneBusy = false;
  let bound = false;

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
    updateControls();
  }

  // Recompute enabled/disabled state for all scene save/recall buttons.
  function updateControls() {
    const offline = !isInteractable();
    const unsupported = !supportsScenes();
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
    if (!supportsScenes()) {
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
    if (!supportsScenes()) {
      setSceneStatus('muted', 'Scene storage is unavailable on this firmware.');
      setStatus(
        'warn',
        'Scenes unavailable',
        'This firmware does not expose scene storage to the browser.'
      );
      return;
    }
    if (!isInteractable() || sceneBusy) return;
    const slotInfo = sceneSlotElements[slotIndex];
    if (!slotInfo) return;
    const name = slotInfo.nameInput?.value.trim();
    sceneBusy = true;
    updateControls();
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
      updateControls();
    }
  }

  // Recall one saved scene back into the live deck state.
  async function handleSceneRecall(slotIndex) {
    if (!supportsScenes()) {
      setSceneStatus('muted', 'Scene storage is unavailable on this firmware.');
      setStatus(
        'warn',
        'Scenes unavailable',
        'This firmware does not expose scene storage to the browser.'
      );
      return;
    }
    if (!isInteractable() || sceneBusy) return;
    sceneBusy = true;
    updateControls();
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
      updateControls();
    }
  }

  function initializeSceneGrid() {
    if (!sceneGrid) return;
    for (let slotIndex = 0; slotIndex < sceneSlotCount; slotIndex++) {
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

  function syncSupportCopy() {
    if (!isInteractable()) {
      setSceneStatus('muted', 'Connect to see whether scene storage is available.');
      return;
    }
    if (!supportsScenes()) {
      setSceneStatus('muted', 'Scene storage is unavailable on this firmware.');
    }
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

  function bind() {
    if (bound) return;
    bound = true;
    initializeSceneGrid();
    updateControls();
  }

  return {
    bind,
    onScene,
    updateControls,
    refreshSceneList,
    syncSupportCopy
  };
}
