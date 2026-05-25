import { VirtualGrid, VirtualList } from '../virtualizers.js';

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
    const label = document.createElement('span');
    label.textContent = `${labelPrefix} ${String(i + 1).padStart(2, '0')}`;
    const progress = document.createElement('progress');
    progress.max = 127;
    progress.value = 0;
    const value = document.createElement('span');
    value.className = 'meter-value';
    value.textContent = '0';
    wrap.append(label, progress, value);
    container.appendChild(wrap);
    meters.push({ wrap, progress, value });
  }
  return meters;
}

export function createSlotWorkspaceController({
  runtime,
  slotState,
  slotContainer = null,
  efAssignmentGrid = null,
  envelopeContainer = null,
  slotTypeAbbreviations = {},
  slotRowHeight = 82,
  efRowHeight = 108,
  onSelectSlot = () => {},
  onTelemetryPainted = () => {},
  onSlotsChanged = () => {},
  performerPanel = null
} = {}) {
  let envMeters = [];

  const slotVirtualizer = new VirtualGrid(slotContainer, {
    columns: 6,
    rowHeight: slotRowHeight,
    render: renderSlotButton
  });
  const efVirtualizer = new VirtualList(efAssignmentGrid, {
    itemHeight: efRowHeight,
    render: renderEfRow
  });

  slotVirtualizer.setData([]);

  function bind() {
    slotContainer?.addEventListener('keydown', (event) => {
      if (!slotState.slots.length) return;
      const { key } = event;
      const columns = slotVirtualizer.columns;
      let next = slotState.selected;
      if (key === 'ArrowRight') next += 1;
      else if (key === 'ArrowLeft') next -= 1;
      else if (key === 'ArrowUp') next -= columns;
      else if (key === 'ArrowDown') next += columns;
      else if (key === 'Home') next = 0;
      else if (key === 'End') next = slotState.slots.length - 1;
      else return;
      event.preventDefault();
      selectSlot(Math.max(0, Math.min(slotState.slots.length - 1, next)));
    });
  }

  function rebuildMeters(count, labelPrefix = 'EF') {
    envMeters = initializeMeters(envelopeContainer, count, labelPrefix);
  }

  function syncConfig(staged) {
    slotState.slots = staged?.slots ?? [];
    slotState.efSlots = staged?.efSlots ?? [];
    slotState.staged = staged;
    if (slotState.selected >= slotState.slots.length) {
      slotState.selected = Math.max(0, slotState.slots.length - 1);
    }
    slotVirtualizer.setData(slotState.slots);
    slotVirtualizer.highlight(slotState.selected);
    efVirtualizer.setData(slotState.efSlots);
    onSlotsChanged(slotState.slots);
  }

  function selectSlot(index) {
    const maxIndex = Math.max(0, slotState.slots.length - 1);
    slotState.selected = Math.min(Math.max(0, index), maxIndex);
    slotVirtualizer.highlight(slotState.selected);
    slotVirtualizer.scrollToIndex(slotState.selected);
    performerPanel?.highlightSelectedSlot?.();
    onSelectSlot(slotState.selected);
  }

  function paintTelemetry(frame) {
    if (!Array.isArray(frame?.slots)) return;
    slotState.telemetry = frame;
    slotVirtualizer.updateTelemetry(frame.slots);
    frame.envelopes?.forEach((value, idx) => {
      const entry = envMeters[idx];
      if (!entry) return;
      entry.progress.value = value;
      entry.value.textContent = String(value);
    });
    performerPanel?.paintTelemetry?.(frame);
    onTelemetryPainted(frame);
  }

  function updateTakeoverGuards(slots) {
    if (!Array.isArray(slots)) return;
    const guardOn = [];
    const guardOff = [];
    slots.forEach((slot, idx) => {
      if (slot?.takeover) guardOff.push(idx);
      else guardOn.push(idx);
    });
    if (guardOn.length) runtime.setPotGuard(guardOn, true);
    if (guardOff.length) runtime.setPotGuard(guardOff, false);
  }

  function renderSlotButton(el, index, slot) {
    el.className = 'slot-button';
    el.tabIndex = -1;
    el.innerHTML = '';
    el.dataset.index = String(index);
    const label = document.createElement('span');
    label.className = 'slot-label';
    label.textContent = `S${String(index + 1).padStart(2, '0')}`;
    const state = document.createElement('span');
    state.className = 'slot-state';
    state.textContent = slotTypeAbbreviations[slot?.type] ?? slot?.type ?? '—';
    state.title = slot?.type ?? 'Unassigned';
    el.dataset.slotType = slotTypeCssToken(slot?.type);
    const toggle = document.createElement('button');
    toggle.type = 'button';
    toggle.className = 'takeover';
    toggle.textContent = slot?.takeover ? 'PK' : 'IM';
    toggle.title = slot?.takeover
      ? 'Browser-only pickup guard enabled'
      : 'Immediate local response';
    toggle.setAttribute('aria-label', slot?.takeover ? 'Pickup mode' : 'Immediate mode');
    toggle.onclick = () => {
      const next = !slot?.takeover;
      runtime.setLocalSlotMeta(index, { takeover: next });
      runtime.setPotGuard([index], !next);
    };
    el.append(label, state, toggle);
    el.onclick = () => selectSlot(index);
    el.setAttribute('role', 'button');
    el.classList.toggle('selected', index === slotState.selected);
  }

  function normalizeFollowerSlotList(item, maxSlotIndex) {
    const source = Array.isArray(item?.slots)
      ? item.slots
      : item?.slot !== undefined && item?.slot !== null
        ? [item.slot]
        : [];
    const seen = new Set();
    const normalized = [];
    source.forEach((candidate) => {
      const numeric = Number(candidate);
      if (!Number.isFinite(numeric)) return;
      const rounded = Math.round(numeric);
      if (rounded < 0) return;
      const bounded = Math.max(0, Math.min(maxSlotIndex, rounded));
      if (seen.has(bounded)) return;
      seen.add(bounded);
      normalized.push(bounded);
    });
    normalized.sort((a, b) => a - b);
    return normalized;
  }

  function toggleFollowerSlotAssignment(followerIndex, slotIndex, maxSlotIndex) {
    runtime.stage((draft) => {
      draft.efSlots = draft.efSlots || [];
      if (!draft.efSlots[followerIndex] || typeof draft.efSlots[followerIndex] !== 'object') {
        draft.efSlots[followerIndex] = { slots: [] };
      }
      const current = normalizeFollowerSlotList(draft.efSlots[followerIndex], maxSlotIndex);
      const next = current.includes(slotIndex)
        ? current.filter((value) => value !== slotIndex)
        : [...current, slotIndex].sort((a, b) => a - b);
      draft.efSlots[followerIndex].slots = next;
      delete draft.efSlots[followerIndex].slot;
      return draft;
    });
  }

  function renderEfRow(el, index, item) {
    el.className = 'ef-row';
    el.innerHTML = '';
    const maxSlotIndex = Math.max(0, slotState.slots.length - 1);
    const normalizedSlots = normalizeFollowerSlotList(item, maxSlotIndex);
    const label = document.createElement('span');
    label.className = 'ef-row-label';
    label.textContent = `EF ${index + 1}`;
    const summary = document.createElement('span');
    summary.className = 'ef-row-summary';
    summary.textContent = normalizedSlots.length
      ? `${normalizedSlots.length} assigned`
      : 'Unassigned';
    const chipGrid = document.createElement('div');
    chipGrid.className = 'ef-chip-grid';
    for (let slotIndex = 0; slotIndex <= maxSlotIndex; slotIndex += 1) {
      const chip = document.createElement('button');
      chip.type = 'button';
      chip.className = 'ef-slot-chip';
      chip.dataset.slotType = slotTypeCssToken(slotState.slots[slotIndex]?.type);
      chip.dataset.slotIndex = String(slotIndex);
      chip.classList.toggle('assigned', normalizedSlots.includes(slotIndex));
      chip.setAttribute('aria-pressed', normalizedSlots.includes(slotIndex) ? 'true' : 'false');
      chip.textContent = `S${String(slotIndex + 1).padStart(2, '0')}`;
      chip.title = slotState.slots[slotIndex]?.type
        ? `Toggle ${slotState.slots[slotIndex].type} slot ${slotIndex + 1}`
        : `Toggle slot ${slotIndex + 1}`;
      chip.addEventListener('click', () =>
        toggleFollowerSlotAssignment(index, slotIndex, maxSlotIndex)
      );
      chipGrid.appendChild(chip);
    }
    el.append(label, summary, chipGrid);
  }

  return {
    bind,
    rebuildMeters,
    syncConfig,
    selectSlot,
    paintTelemetry,
    updateTakeoverGuards
  };
}
