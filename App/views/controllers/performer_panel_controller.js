import {
  describeSlotModulation,
  formatSlotModulationTitle,
  renderSlotModulationBadges
} from '../slot_modulation_summary.js';
import { applyModulationIdentity } from '../modulation_identity.js';

const PROFILE_SLOT_LABELS = ['A', 'B', 'C', 'D'];
const SLOT_ACTIVITY_DECAY_MS = 280;

export function formatStageClockState(clock, legacyFrame = {}) {
  if (!clock || typeof clock !== 'object') return { text: 'Waiting', health: 'unknown' };
  const source = String(clock.source ?? '').toLowerCase();
  const followsExternal = clock.follow_external === true;
  const externalMissing = followsExternal && clock.external_signal === false;
  const sourceLabel = source === 'external' ? 'EXT' : source === 'internal' ? 'INT' : 'CLOCK';
  const bpm = Number(
    source === 'external'
      ? clock.external_bpm ?? clock.bpm ?? legacyFrame.clock_bpm
      : clock.tapped_bpm ?? clock.bpm ?? legacyFrame.clock_bpm
  );
  const bpmText = Number.isFinite(bpm) && bpm > 0 ? `${bpm.toFixed(1)} BPM` : null;
  const runningText = typeof clock.running === 'boolean'
    ? clock.running
      ? 'Running'
      : 'Stopped'
    : null;

  if (externalMissing) {
    const fallbackLabel = source === 'internal' ? 'INT fallback' : 'CLOCK';
    return {
      text: [fallbackLabel, bpmText, 'Waiting for EXT'].filter(Boolean).join(' · '),
      health: 'waiting-external'
    };
  }

  return {
    text: [sourceLabel, bpmText, runningText].filter(Boolean).join(' · ') || 'Waiting',
    health: source === 'external' ? 'external' : source === 'internal' ? 'internal' : 'idle'
  };
}

function slotTypeCssToken(type) {
  if (typeof type !== 'string' || !type.trim()) return 'off';
  return type.toLowerCase().replace(/[^a-z0-9]+/g, '');
}

function initializeMeters(container, count, labelPrefix) {
  if (!container) return [];
  container.innerHTML = '';
  const meters = [];
  for (let i = 0; i < count; i += 1) {
    const wrap = document.createElement('div');
    wrap.className = 'meter';
    applyModulationIdentity(wrap, 'ef', i);
    const label = document.createElement('span');
    label.className = 'meter-label';
    const name = document.createElement('strong');
    name.textContent = `${labelPrefix} ${String(i + 1).padStart(2, '0')}`;
    const state = document.createElement('small');
    state.className = 'meter-state';
    state.textContent = 'STATUS --';
    const routes = document.createElement('button');
    routes.type = 'button';
    routes.className = 'meter-routes';
    routes.textContent = 'No routes';
    routes.disabled = true;
    routes.setAttribute('aria-expanded', 'false');
    const destinations = document.createElement('small');
    destinations.className = 'meter-destinations';
    destinations.hidden = true;
    routes.addEventListener('click', () => {
      const expanded = routes.getAttribute('aria-expanded') === 'true';
      routes.setAttribute('aria-expanded', expanded ? 'false' : 'true');
      destinations.hidden = expanded;
    });
    label.append(name, state, routes, destinations);
    const progress = document.createElement('progress');
    progress.max = 127;
    progress.value = 0;
    progress.setAttribute('aria-label', `${labelPrefix} ${i + 1} level`);
    const value = document.createElement('span');
    value.className = 'meter-value';
    value.textContent = '00';
    wrap.append(label, progress, value);
    container.appendChild(wrap);
    meters.push({ wrap, progress, value, state, routes, destinations });
  }
  return meters;
}

export function createPerformerPanelController({
  elements = {},
  runtime,
  localManifest,
  resolveDeviceName,
  resolveFirmwareVersion,
  slotTypeAbbreviations = {},
  connect = () => {},
  getConnectionStage = () => 'disconnected',
  getConnectionText = () => 'Disconnected',
  getActiveProfileSlot = () => 0,
  getDeviceActiveProfileSlot = () => null,
  getProfileName = () => '',
  getSceneState = () => null,
  getLastRecalledScene = () => null,
  setActiveProfileSlot = () => {},
  loadProfile = () => {},
  recallScene = () => {},
  getSelectedSlot = () => 0,
  selectSlot = () => {},
  setStatus = () => {},
  openPanicHelp = () => {}
} = {}) {
  const {
    panel = null,
    connectBtn = null,
    connectionState = null,
    dirtyState = null,
    deviceName = null,
    fwVersion = null,
    profileSummary = null,
    sceneSummary = null,
    clockState = null,
    midiOutput = null,
    slotFocus = null,
    profileSelect = null,
    profileLoadBtn = null,
    sceneSelect = null,
    sceneRecallBtn = null,
    draftBlockedNotice = null,
    panicHelpBtn = null,
    slotGrid = null,
    envelopeContainer = null,
    envelopeCount = null
  } = elements;

  let slotCells = [];
  let envMeters = [];
  let renderedSlots = [];
  let latestTelemetry = null;
  let previousSlotValues = [];
  let slotActivityTimers = [];

  function resetSlotActivity() {
    slotActivityTimers.forEach((timer) => clearTimeout(timer));
    slotActivityTimers = [];
    previousSlotValues = [];
    slotCells.forEach((cell) => cell.classList.remove('active'));
  }

  function pulseSlotActivity(cell, index) {
    cell.classList.add('active');
    clearTimeout(slotActivityTimers[index]);
    slotActivityTimers[index] = setTimeout(() => {
      cell.classList.remove('active');
      slotActivityTimers[index] = null;
    }, SLOT_ACTIVITY_DECAY_MS);
  }

  function refreshSlotFocus() {
    if (!slotFocus) return;
    const index = Math.max(0, Number(getSelectedSlot()) || 0);
    const slot = renderedSlots[index] ?? {};
    const value = latestTelemetry?.slots?.[index];
    const type = slotTypeAbbreviations[slot?.type] ?? slot?.type_name ?? slot?.type ?? 'OFF';
    const data1 = Number(slot?.data1 ?? slot?.cc ?? slot?.note);
    const destination = Number.isFinite(data1) && !['OFF', 'PB'].includes(String(type).toUpperCase())
      ? `${type}${data1}`
      : type;
    const channel = Number(slot?.midiChannel ?? slot?.channel);
    const channelText = Number.isFinite(channel) ? `Ch ${channel}` : 'No channel';
    const valueText = Number.isFinite(Number(value)) ? `OUT ${Number(value)}` : 'OUT --';
    const modulation = describeSlotModulation(slot);
    const sourceValues = modulation.flatMap((badge) => {
      const efMatch = badge.match(/^E(\d+)$/);
      if (efMatch) {
        const sourceValue = latestTelemetry?.envelopes?.[Number(efMatch[1]) - 1];
        return [
          Number.isFinite(Number(sourceValue))
            ? `${badge} ${Math.round(Number(sourceValue))}`
            : badge
        ];
      }
      const lfoMatch = badge.match(/^L(\d+)$/);
      if (lfoMatch) {
        const sourceValue = latestTelemetry?.lfos?.[Number(lfoMatch[1]) - 1];
        return [
          Number.isFinite(Number(sourceValue))
            ? `${badge} ${Number(sourceValue).toFixed(2)}`
            : badge
        ];
      }
      return [badge === 'A' ? 'A enabled' : badge];
    });
    const slotName = typeof slot?.label === 'string' ? slot.label.trim() : '';
    slotFocus.textContent = [
      slotName ? `${slotName} · S${index + 1}` : `Slot ${index + 1}`,
      destination,
      channelText,
      valueText,
      ...(sourceValues.length
        ? sourceValues
        : [modulation.length ? modulation.join(' + ') : 'No modulation'])
    ].join(' · ');
  }

  function highlightSelectedSlot() {
    const selected = Math.max(0, Number(getSelectedSlot()) || 0);
    slotCells.forEach((cell, index) => {
      const isSelected = index === selected;
      cell.classList.toggle('selected', isSelected);
      cell.setAttribute('aria-pressed', isSelected ? 'true' : 'false');
    });
    refreshSlotFocus();
  }

  function bind() {
    connectBtn?.addEventListener('click', () => connect());
    profileSelect?.addEventListener('change', () => {
      setActiveProfileSlot(Number(profileSelect.value));
      refresh();
    });
    profileLoadBtn?.addEventListener('click', () => loadProfile());
    sceneSelect?.addEventListener('change', () => refresh());
    sceneRecallBtn?.addEventListener('click', () => recallScene(Number(sceneSelect?.value ?? 0)));
    panicHelpBtn?.addEventListener('click', () => {
      setStatus(
        'warn',
        'Panic & recovery',
        'Press Ctrl0 + Ctrl1 + Ctrl2 for the hardware panic baseline. Flash and recovery guidance is open on screen.'
      );
      openPanicHelp();
    });
  }

  function setVisible(visible) {
    panel?.toggleAttribute('hidden', !visible);
  }

  function rebuildMeters(count) {
    envMeters = initializeMeters(envelopeContainer, count, 'EF');
    if (envelopeCount) {
      envelopeCount.textContent = `${count} ${count === 1 ? 'follower' : 'followers'}`;
    }
    refreshEnvelopeRoutes();
  }

  function refreshEnvelopeRoutes() {
    const state = runtime?.getState?.() ?? {};
    const config = state.staged ?? state.live ?? {};
    const efSlots = Array.isArray(config?.efSlots) ? config.efSlots : [];
    const slots = Array.isArray(config?.slots) ? config.slots : [];
    envMeters.forEach((entry, idx) => {
      const routeEntry = efSlots.find(
        (candidate, entryIndex) => Number(candidate?.index ?? entryIndex) === idx
      );
      const assignedIndices = Array.isArray(routeEntry?.slots)
        ? Array.from(
            new Set(
              routeEntry.slots
                .map(Number)
                .filter(
                  (slotIndex) =>
                    Number.isInteger(slotIndex) && slotIndex >= 0 && slotIndex < slots.length
                )
            )
          )
        : slots.flatMap((slot, slotIndex) => {
            const efIndex = Number(slot?.efIndex ?? slot?.ef_index ?? slot?.ef?.index);
            return Number.isFinite(efIndex) && Math.round(efIndex) === idx ? [slotIndex] : [];
          });
      const destinations = assignedIndices
        .sort((left, right) => left - right)
        .map((slotIndex) => {
          const slotLabel =
            typeof slots[slotIndex]?.label === 'string' ? slots[slotIndex].label.trim() : '';
          return slotLabel ? `${slotLabel} (S${slotIndex + 1})` : `S${slotIndex + 1}`;
        });
      const assigned = destinations.length;
      entry.routes.textContent = assigned
        ? `→ ${assigned} ${assigned === 1 ? 'slot' : 'slots'}`
        : 'No routes';
      entry.routes.disabled = !assigned;
      entry.routes.setAttribute(
        'aria-label',
        assigned
          ? `Show EF ${idx + 1} destinations`
          : `EF ${idx + 1} has no route destinations`
      );
      entry.destinations.textContent = destinations.join(' · ');
      if (!assigned) {
        entry.destinations.hidden = true;
        entry.routes.setAttribute('aria-expanded', 'false');
      }
      entry.wrap.title = assigned
        ? `Envelope follower ${idx + 1} → ${destinations.join(', ')}`
        : `Envelope follower ${idx + 1} has no configured routes`;
    });
  }

  function renderSlots(slots) {
    if (!slotGrid) return;
    const source = Array.isArray(slots) ? slots : [];
    renderedSlots = source;
    if (slotCells.length === source.length && slotCells.length) {
      slotCells.forEach((cell, index) => {
        const slot = source[index];
        const state = cell.querySelector('.stage-slot-type');
        if (state) state.textContent = slotTypeAbbreviations[slot?.type] ?? slot?.type ?? '-';
        const modulation = cell.querySelector('.stage-slot-modulation');
        const name = cell.querySelector('.stage-slot-name');
        const slotName = typeof slot?.label === 'string' ? slot.label.trim() : '';
        if (name) {
          name.textContent = slotName;
          name.hidden = !slotName;
        }
        cell.setAttribute(
          'aria-label',
          slotName ? `${slotName}, slot ${index + 1}` : `Slot ${index + 1}`
        );
        const badges = describeSlotModulation(slot);
        if (modulation) {
          renderSlotModulationBadges(modulation, badges);
          modulation.title = formatSlotModulationTitle(badges);
          modulation.setAttribute('aria-hidden', 'true');
        }
        cell.title = [slotName, formatSlotModulationTitle(badges)].filter(Boolean).join(' · ');
        cell.dataset.slotType = slotTypeCssToken(slot?.type);
      });
      highlightSelectedSlot();
      return;
    }

    resetSlotActivity();
    slotGrid.innerHTML = '';
    slotCells = source.map((slot, index) => {
      const cell = document.createElement('button');
      cell.type = 'button';
      cell.className = 'stage-slot-cell';
      cell.dataset.index = String(index);
      cell.dataset.slotType = slotTypeCssToken(slot?.type);
      cell.setAttribute('aria-pressed', 'false');
      cell.addEventListener('click', () => selectSlot(index));

      const label = document.createElement('span');
      label.className = 'stage-slot-label';
      label.textContent = String(index + 1).padStart(2, '0');

      const type = document.createElement('span');
      type.className = 'stage-slot-type';
      type.textContent = slotTypeAbbreviations[slot?.type] ?? slot?.type ?? '-';

      const value = document.createElement('span');
      value.className = 'stage-slot-value';
      value.textContent = '--';

      const modulation = document.createElement('span');
      modulation.className = 'stage-slot-modulation';
      const badges = describeSlotModulation(slot);
      renderSlotModulationBadges(modulation, badges);
      modulation.title = formatSlotModulationTitle(badges);
      modulation.setAttribute('aria-hidden', 'true');
      const name = document.createElement('span');
      name.className = 'stage-slot-name';
      name.textContent = typeof slot?.label === 'string' ? slot.label.trim() : '';
      name.hidden = !name.textContent;
      cell.setAttribute(
        'aria-label',
        name.textContent ? `${name.textContent}, slot ${index + 1}` : `Slot ${index + 1}`
      );
      cell.title = [name.textContent, formatSlotModulationTitle(badges)]
        .filter(Boolean)
        .join(' · ');

      cell.append(label, type, value, modulation, name);
      slotGrid.appendChild(cell);
      return cell;
    });
    highlightSelectedSlot();
  }

  function paintTelemetry(frame) {
    if (!frame || typeof frame !== 'object') return;
    latestTelemetry = {
      ...(latestTelemetry ?? {}),
      ...frame,
      slots: Array.isArray(frame.slots) ? frame.slots : latestTelemetry?.slots,
      envelopes: Array.isArray(frame.envelopes) ? frame.envelopes : latestTelemetry?.envelopes,
      efStatus: Array.isArray(frame.efStatus) ? frame.efStatus : latestTelemetry?.efStatus,
      lfos: Array.isArray(frame.lfos) ? frame.lfos : latestTelemetry?.lfos,
      clock:
        frame.clock && typeof frame.clock === 'object'
          ? { ...(latestTelemetry?.clock ?? {}), ...frame.clock }
          : latestTelemetry?.clock
    };

    latestTelemetry.envelopes?.forEach((value, idx) => {
      const entry = envMeters[idx];
      if (!entry) return;
      entry.progress.value = value;
      entry.value.textContent = String(value).padStart(2, '0');
    });
    latestTelemetry.efStatus?.forEach((active, idx) => {
      const entry = envMeters[idx];
      if (!entry) return;
      const isActive = Boolean(Number(active));
      entry.wrap.dataset.state = isActive ? 'active' : 'inactive';
      entry.state.textContent = isActive ? 'ACTIVE' : 'IDLE';
    });

    frame.slots?.forEach((value, idx) => {
      const cell = slotCells[idx];
      if (!cell) return;
      const numeric = Number(value);
      const previous = previousSlotValues[idx];
      const valid = Number.isFinite(numeric);
      if (valid && Number.isFinite(previous) && numeric !== previous) {
        pulseSlotActivity(cell, idx);
      }
      previousSlotValues[idx] = valid ? numeric : null;
      const valueEl = cell.querySelector('.stage-slot-value');
      if (valueEl) valueEl.textContent = valid ? String(numeric) : '--';
    });

    if (clockState) {
      const clock = latestTelemetry.clock;
      const described = formatStageClockState(
        clock
          ? {
              ...clock,
              running:
                typeof clock.running === 'boolean'
                  ? clock.running
                  : latestTelemetry.clock_running
            }
          : null,
        latestTelemetry
      );
      clockState.textContent = described.text;
      clockState.dataset.clockHealth = described.health;
    }
    if (midiOutput) {
      const enabled = frame?.usb_midi_enabled ?? frame?.midi_output_enabled;
      if (typeof enabled === 'boolean') midiOutput.textContent = enabled ? 'Enabled' : 'Muted';
    }
    refreshSlotFocus();
  }

  function refresh({
    profileLoadDisabled = true,
    sceneRecallDisabled = true,
    connected = false
  } = {}) {
    const manifest = runtime?.getState?.().manifest ?? localManifest;
    const dirtyNow = Boolean(runtime?.getState?.().dirty);

    if (connectionState) {
      connectionState.textContent = getConnectionText();
      connectionState.dataset.stage = getConnectionStage();
    }
    if (connectBtn) {
      connectBtn.textContent = connected ? 'Reconnect' : 'Connect';
    }
    if (dirtyState) {
      dirtyState.textContent = dirtyNow ? 'Dirty' : 'Clean';
      dirtyState.dataset.state = dirtyNow ? 'dirty' : 'clean';
    }
    if (draftBlockedNotice) draftBlockedNotice.hidden = !dirtyNow;
    if (deviceName) deviceName.textContent = resolveDeviceName?.(manifest) ?? '-';
    if (fwVersion) fwVersion.textContent = resolveFirmwareVersion?.(manifest) ?? 'unknown';
    if (profileSummary) {
      const active = getDeviceActiveProfileSlot();
      if (!connected || !Number.isInteger(Number(active))) {
        profileSummary.textContent = 'Unavailable';
      } else {
        const index = Number(active);
        const fallback = `Profile ${slotLabel(index)}`;
        const name = getProfileName(index)?.trim?.() ?? '';
        profileSummary.textContent = name ? `${name} · ${fallback}` : fallback;
      }
    }
    if (sceneSummary) {
      const recalled = getLastRecalledScene();
      if (!recalled || !Number.isInteger(Number(recalled.slot))) {
        sceneSummary.textContent = 'No browser recall';
      } else {
        const index = Number(recalled.slot);
        const fallback = `Scene ${index + 1}`;
        const name = recalled.name?.trim?.() ?? '';
        sceneSummary.textContent = name ? `${name} · ${fallback}` : fallback;
      }
    }
    if (midiOutput) {
      if (!connected) midiOutput.textContent = 'Offline';
      else if (!['Enabled', 'Muted'].includes(midiOutput.textContent)) {
        midiOutput.textContent = 'Connected';
      }
    }
    if (profileSelect) {
      profileSelect.value = String(getActiveProfileSlot());
      Array.from(profileSelect.options).forEach((option) => {
        const index = Number(option.value);
        const label = slotLabel(index);
        const name = getProfileName(index)?.trim?.() ?? '';
        option.textContent = name ? `${name} · Profile ${label}` : `Profile ${label}`;
      });
    }
    if (profileLoadBtn) {
      profileLoadBtn.disabled = dirtyNow || Boolean(profileLoadDisabled);
      const index = getActiveProfileSlot();
      const fallback = `Profile ${slotLabel(index)}`;
      const name = getProfileName(index)?.trim?.() ?? '';
      profileLoadBtn.textContent = `Recall ${name ? `${name} (${fallback})` : fallback} now`;
    }
    Array.from(sceneSelect?.options ?? []).forEach((option) => {
      const index = Number(option.value);
      const state = getSceneState(index);
      option.textContent = state?.available && state?.name
        ? `${state.name} · Scene ${index + 1}`
        : `Scene ${index + 1}`;
    });
    if (sceneRecallBtn) {
      sceneRecallBtn.disabled = dirtyNow || Boolean(sceneRecallDisabled);
      const index = Number(sceneSelect?.value ?? 0);
      const state = getSceneState(index);
      const fallback = `Scene ${index + 1}`;
      const target = state?.available && state?.name ? `${state.name} (${fallback})` : fallback;
      sceneRecallBtn.textContent = `Recall ${target} now`;
    }
    refreshEnvelopeRoutes();
  }

  function slotLabel(index) {
    return PROFILE_SLOT_LABELS[index] ?? PROFILE_SLOT_LABELS[0];
  }

  return {
    bind,
    setVisible,
    rebuildMeters,
    renderSlots,
    paintTelemetry,
    refresh,
    highlightSelectedSlot,
    slotLabel
  };
}
