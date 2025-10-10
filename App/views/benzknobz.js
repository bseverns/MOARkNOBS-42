import { createRuntime } from '../runtime.js';

const localManifest = {
  ui_version: '2024.08.01',
  schema_version: '1.3.0',
  slot_count: 42,
  max_table_lengths: {
    ledColors: 42,
    efSlots: 6
  }
};

const runtime = createRuntime({
  schemaUrl: './config_schema.json',
  localManifest
});

const sharedResizeObserver =
  typeof ResizeObserver === 'function'
    ? new ResizeObserver((entries) => {
        for (const entry of entries) {
          entry.target.__resizeCallback?.(entry);
        }
      })
    : null;

window.addEventListener('DOMContentLoaded', () => {
  const statusEl = document.getElementById('status');
  const statusLabel = document.getElementById('status-label');
  const statusMessage = statusEl?.querySelector('.status-message');
  const connectBtn = document.getElementById('connect');
  const applyBtn = document.getElementById('apply');
  const rollbackBtn = document.getElementById('rollback');
  const slotContainer = document.getElementById('slots');
  const envContainer = document.getElementById('envelopes');
  const diffPanel = document.getElementById('diff-panel');
  const diffOutput = document.getElementById('diff-output');
  const dirtyBadge = document.getElementById('dirty-badge');
  const connectionPill = document.getElementById('connection-pill');
  const headerStatus = document.getElementById('header-status');
  const exportPresetBtn = document.getElementById('export-preset');
  const importPresetBtn = document.getElementById('import-preset');
  const simulatorToggle = document.getElementById('simulator-toggle');
  const ledGrid = document.getElementById('led-grid');
  const formContainer = document.getElementById('form');
  const filterTypeEl = document.getElementById('filter-type');
  const filterFreqEl = document.getElementById('filter-freq');
  const filterQEl = document.getElementById('filter-q');
  const argEnableEl = document.getElementById('arg-enable');
  const argAEl = document.getElementById('arg-a');
  const argBEl = document.getElementById('arg-b');
  const efAssignmentGrid = document.querySelector('#ef-assignment-card .ef-grid');
  const slotDetailIndex = document.getElementById('slot-detail-index');
  const slotDetailStatus = document.getElementById('slot-detail-status');
  const slotDetailType = document.getElementById('slot-detail-type');
  const slotDetailChannel = document.getElementById('slot-detail-channel');
  const slotDetailData = document.getElementById('slot-detail-data');
  const slotDetailEnvelope = document.getElementById('slot-detail-envelope');
  const slotDetailArg = document.getElementById('slot-detail-arg');
  const slotDetailValue = document.getElementById('slot-detail-value');
  const deviceMonitor = document.getElementById('device-monitor');
  const logEl = document.getElementById('log');
  const migrationDialog = document.getElementById('migration-dialog');
  const migrationPreview = document.getElementById('migration-preview');
  const migrationApply = document.getElementById('migration-apply');
  const migrationCancel = document.getElementById('migration-cancel');
  const migrationExport = document.getElementById('migration-export');

  const slotState = {
    slots: [],
    efSlots: [],
    staged: null,
    selected: 0,
    telemetry: null
  };

  const slotVirtualizer = new VirtualGrid(slotContainer, {
    columns: 6,
    rowHeight: 72,
    render: renderSlotButton
  });

  const efVirtualizer = new VirtualList(efAssignmentGrid, {
    itemHeight: 48,
    render: renderEfRow
  });

  const envMeters = initializeMeters(envContainer, localManifest.max_table_lengths.efSlots, 'EF');
  slotVirtualizer.setData([]);

  connectBtn?.addEventListener('click', async () => {
    try {
      if (connectionPill) {
        connectionPill.dataset.stage = 'handshake';
        connectionPill.textContent = 'Handshaking…';
      }
      await runtime.connect();
    } catch (err) {
      setStatus('err', 'Connect failed', err.message || String(err));
    }
  });

  applyBtn?.addEventListener('click', async () => {
    try {
      setStatus('warn', 'Applying…', 'Waiting for firmware ACK');
      await runtime.apply();
      setStatus('ok', 'Synced', 'Device acknowledged the staged edits.');
    } catch (err) {
      setStatus('err', 'Apply failed', err.message || String(err));
    }
  });

  rollbackBtn?.addEventListener('click', async () => {
    await runtime.rollback();
    setStatus('warn', 'Rolled back', 'Local edits were discarded.');
  });

  simulatorToggle?.addEventListener('click', () => {
    const toggled = simulatorToggle.classList.toggle('active');
    runtime.useSimulator(toggled);
    simulatorToggle.textContent = toggled ? 'Stop simulator' : 'Start simulator';
    setStatus(toggled ? 'ok' : 'warn', toggled ? 'Simulator armed' : 'Simulator idle', toggled ? 'Replay frames without hardware.' : 'Connect to the physical deck.');
  });

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

  filterTypeEl?.addEventListener('change', () => stageFilter('type', filterTypeEl.value));
  filterFreqEl?.addEventListener('change', () => stageFilter('freq', Number(filterFreqEl.value)));
  filterQEl?.addEventListener('change', () => stageFilter('q', Number(filterQEl.value)));
  argEnableEl?.addEventListener('change', () => stageArg('enable', argEnableEl.value === 'true'));
  argAEl?.addEventListener('change', () => stageArg('a', Number(argAEl.value)));
  argBEl?.addEventListener('change', () => stageArg('b', Number(argBEl.value)));

  exportPresetBtn?.addEventListener('click', () => {
    const { staged } = runtime.getState();
    const blob = new Blob([JSON.stringify(staged, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = `moarknobz-preset-${new Date().toISOString().replace(/[:.]/g, '-')}.json`;
    anchor.click();
    URL.revokeObjectURL(url);
  });

  importPresetBtn?.addEventListener('click', () => {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json,application/json';
    input.onchange = async () => {
      const file = input.files?.[0];
      if (!file) return;
      const text = await file.text();
      try {
        const json = JSON.parse(text);
        runtime.stage(() => json);
      } catch (err) {
        setStatus('err', 'Import failed', err.message || String(err));
      }
    };
    input.click();
  });

  migrationApply?.addEventListener('click', () => migrationDialog?.close('apply'));
  migrationCancel?.addEventListener('click', () => migrationDialog?.close('cancel'));
  migrationExport?.addEventListener('click', () => exportPresetBtn?.click());
  migrationDialog?.addEventListener('close', () => {
    if (!migrationDialog?.returnValue || migrationDialog.returnValue === 'apply') {
      setStatus('warn', 'Migration ready', 'Review staged adapter before applying.');
    } else {
      setStatus('warn', 'Schema mismatch', 'Update firmware or UI so schemas agree.');
    }
  });

  runtime.on('status', ({ level, message }) => {
    const map = { error: 'err', warn: 'warn', info: 'ok', ok: 'ok' };
    const state = map[level] || 'warn';
    setStatus(state, level?.toUpperCase?.() || 'NOTE', message);
  });
  runtime.on('telemetry', (frame) => {
    slotState.telemetry = frame;
    paintTelemetry(frame);
  });
  runtime.on('config', ({ staged, config, dirty }) => {
    slotState.slots = staged?.slots ?? [];
    slotState.efSlots = staged?.efSlots ?? [];
    slotState.staged = staged;
    if (slotState.selected >= slotState.slots.length) {
      slotState.selected = Math.max(0, slotState.slots.length - 1);
    }
    updateHeader(config);
    slotVirtualizer.setData(slotState.slots);
    slotVirtualizer.highlight(slotState.selected);
    efVirtualizer.setData(slotState.efSlots);
    updateDiff(dirty);
    markDirty(dirty);
    populateDetail();
    populateFilter(staged);
    populateArg(staged);
    renderLedGrid(staged);
    updateTakeoverGuards(slotState.slots);
  });
  runtime.on('manifest', (manifest) => {
    updateHeaderManifest(manifest);
    renderDeviceMonitor(manifest);
  });
  runtime.on('log', (line) => {
    if (!logEl) return;
    logEl.textContent += `${line}\n`;
  });
  runtime.on('validation-error', (errors) => {
    diffPanel?.removeAttribute('hidden');
    diffOutput.textContent = `Schema violations:\n${errors.map((e) => `• ${e.instancePath || '/'} ${e.message}`).join('\n')}`;
  });
  runtime.on('applied', ({ checksum }) => {
    diffPanel?.setAttribute('hidden', '');
    diffOutput.textContent = '';
    setStatus('ok', 'Device synced', `Checksum ${checksum.slice(0, 8)}…`);
  });
  runtime.on('migration-required', ({ from, to, canAdapt }) => {
    if (!migrationDialog || !migrationPreview) return;
    migrationPreview.textContent = `Firmware schema ${from} vs UI ${to}. ${canAdapt ? 'An adapter is available; stage edits then apply.' : 'Export your preset and update firmware/UI to continue.'}`;
    migrationDialog.showModal();
  });
  runtime.on('connected', ({ manifest }) => {
    if (connectionPill) {
      connectionPill.dataset.stage = 'live';
      connectionPill.textContent = `Connected • ${manifest?.fw_version || 'fw?'}`;
    }
    if (applyBtn) applyBtn.disabled = true;
    if (rollbackBtn) rollbackBtn.disabled = true;
    if (exportPresetBtn) exportPresetBtn.disabled = false;
    if (importPresetBtn) importPresetBtn.disabled = false;
    setStatus('ok', 'Connected', 'Schema synced. Stage edits before applying.');
  });
  runtime.on('disconnected', () => {
    if (connectionPill) {
      connectionPill.dataset.stage = 'disconnected';
      connectionPill.textContent = 'Disconnected';
    }
    if (applyBtn) applyBtn.disabled = true;
    if (rollbackBtn) rollbackBtn.disabled = true;
    if (exportPresetBtn) exportPresetBtn.disabled = true;
    if (importPresetBtn) importPresetBtn.disabled = true;
    setStatus('warn', 'Disconnected', 'Reconnect to continue editing.');
  });
  runtime.on('error', (err) => {
    setStatus('err', 'Runtime error', err.message || String(err));
  });
  runtime.on('rollback', () => {
    updateDiff(false);
    markDirty(false);
    setStatus('warn', 'Rollback', 'Local edits were discarded.');
  });

  function setStatus(state, label, message) {
    if (!statusEl || !statusLabel || !statusMessage) return;
    statusEl.dataset.state = state;
    statusLabel.textContent = label;
    statusMessage.textContent = message;
  }

  function markDirty(isDirty) {
    if (dirtyBadge) dirtyBadge.toggleAttribute('hidden', !isDirty);
    if (applyBtn) applyBtn.disabled = !isDirty;
    if (rollbackBtn) rollbackBtn.disabled = !isDirty;
  }

  function updateDiff(isDirty) {
    if (!diffPanel || !diffOutput) return;
    const changes = runtime.diff();
    if (!isDirty || !changes.length) {
      diffPanel.setAttribute('hidden', '');
      diffOutput.textContent = '';
      return;
    }
    diffPanel.removeAttribute('hidden');
    diffOutput.textContent = changes.map(({ path, before, after }) => `${path}: ${JSON.stringify(before)} → ${JSON.stringify(after)}`).join('\n');
  }

  function updateHeader(config) {
    if (!headerStatus) return;
    const manifest = runtime.getState().manifest;
    const ram = manifest?.free?.ram ? `${Math.round(manifest.free.ram / 1024)}k RAM` : 'ram?';
    const flash = manifest?.free?.flash ? `${Math.round(manifest.free.flash / 1024)}k flash` : 'flash?';
    headerStatus.textContent = [manifest?.fw_version || 'fw?', manifest?.schema_version || 'schema?', `${ram} • ${flash}`].join(' • ');
  }

  function updateHeaderManifest(manifest) {
    if (!connectionPill) return;
    connectionPill.dataset.stage = 'live';
    connectionPill.textContent = `Connected • ${manifest.fw_version || 'fw?'}`;
    updateHeader(runtime.getState().live);
  }

  function selectSlot(index) {
    const maxIndex = Math.max(0, slotState.slots.length - 1);
    slotState.selected = Math.min(Math.max(0, index), maxIndex);
    slotVirtualizer.highlight(slotState.selected);
    slotVirtualizer.scrollToIndex(slotState.selected);
    populateDetail();
  }

  function populateDetail() {
    const slot = slotState.slots[slotState.selected];
    const telemetry = slotState.telemetry || {};
    if (slotDetailIndex) slotDetailIndex.textContent = slotState.selected !== undefined ? `Slot ${String(slotState.selected + 1).padStart(2, '0')}` : '—';
    if (slotDetailStatus) slotDetailStatus.textContent = slot?.active ? 'Active' : 'Muted';
    if (slotDetailType) slotDetailType.textContent = slot?.type ?? '—';
    if (slotDetailChannel) slotDetailChannel.textContent = slot?.midiChannel ?? '—';
    if (slotDetailData) slotDetailData.textContent = slot?.data1 ?? '—';
    if (slotDetailEnvelope) slotDetailEnvelope.textContent = slot?.efIndex !== undefined ? `EF ${slot.efIndex + 1}` : '—';
    if (slotDetailArg) slotDetailArg.textContent = slotState.staged?.arg?.method ?? '—';
    const value = Array.isArray(telemetry.slots) ? telemetry.slots[slotState.selected] : null;
    if (slotDetailValue) slotDetailValue.textContent = value ?? '—';
    renderSlotEditor();
  }

  function renderSlotEditor() {
    if (!formContainer) return;
    formContainer.innerHTML = '';
    const slot = slotState.slots[slotState.selected];
    if (!slot) {
      formContainer.textContent = 'Select a slot to edit.';
      return;
    }
    const form = document.createElement('form');
    form.className = 'slot-editor';
    form.addEventListener('submit', (event) => event.preventDefault());

    form.appendChild(makeSelect('MIDI Type', ['OFF', 'CC', 'Note', 'PitchBend', 'ProgramChange', 'Aftertouch', 'ModWheel', 'NRPN', 'RPN', 'SysEx'], slot.type, (value) => stageSlotField(slotState.selected, 'type', value)));
    form.appendChild(makeNumber('MIDI Channel', slot.midiChannel ?? 1, 1, 16, 1, (value) => stageSlotField(slotState.selected, 'midiChannel', value)));
    form.appendChild(makeNumber('Data (CC/Note)', slot.data1 ?? 0, 0, 127, 1, (value) => stageSlotField(slotState.selected, 'data1', value)));
    form.appendChild(makeNumber('Envelope Follow', slot.efIndex ?? 0, 0, localManifest.max_table_lengths.efSlots - 1, 1, (value) => stageSlotField(slotState.selected, 'efIndex', value)));
    form.appendChild(makeToggle('Active', !!slot.active, (value) => stageSlotField(slotState.selected, 'active', value)));
    form.appendChild(makeToggle('Pot mapped', !!slot.pot, (value) => stageSlotField(slotState.selected, 'pot', value)));

    const takeover = makeToggle('Take Control', !!slot.takeover, (value) => {
      stageSlotField(slotState.selected, 'takeover', value);
      runtime.setPotGuard([slotState.selected], !value);
    });
    form.appendChild(takeover);

    formContainer.appendChild(form);
  }

  function makeSelect(labelText, options, current, onChange) {
    const wrap = document.createElement('label');
    wrap.textContent = labelText;
    const select = document.createElement('select');
    options.forEach((opt) => {
      const option = document.createElement('option');
      option.value = opt;
      option.textContent = opt;
      if (opt === current) option.selected = true;
      select.appendChild(option);
    });
    select.onchange = () => onChange(select.value);
    wrap.appendChild(select);
    return wrap;
  }

  function makeNumber(labelText, current, min, max, step, onCommit) {
    const wrap = document.createElement('label');
    wrap.textContent = labelText;
    const input = document.createElement('input');
    input.type = 'number';
    input.min = String(min);
    input.max = String(max);
    input.step = String(step);
    input.value = current;
    input.onchange = () => onCommit(Number(input.value));
    attachCoarseFine(input, step);
    wrap.appendChild(input);
    return wrap;
  }

  function makeToggle(labelText, current, onCommit) {
    const wrap = document.createElement('label');
    wrap.className = 'toggle';
    const input = document.createElement('input');
    input.type = 'checkbox';
    input.checked = current;
    input.onchange = () => onCommit(input.checked);
    const span = document.createElement('span');
    span.textContent = labelText;
    wrap.append(input, span);
    return wrap;
  }

  function attachCoarseFine(input, baseStep) {
    input.addEventListener('keydown', (event) => {
      if (event.key !== 'ArrowUp' && event.key !== 'ArrowDown') return;
      event.preventDefault();
      const delta = (event.key === 'ArrowUp' ? 1 : -1) * (event.shiftKey ? baseStep * 10 : baseStep);
      const next = Math.min(Number(input.max), Math.max(Number(input.min), Number(input.value) + delta));
      input.value = String(next);
      input.dispatchEvent(new Event('change'));
    });
  }

  function stageSlotField(index, key, value) {
    runtime.stage((draft) => {
      draft.slots = draft.slots || [];
      if (!draft.slots[index]) draft.slots[index] = {};
      draft.slots[index][key] = value;
      return draft;
    });
  }

  function populateFilter(staged) {
    if (!staged) return;
    if (filterTypeEl) filterTypeEl.value = staged.filter?.type ?? 'LINEAR';
    if (filterFreqEl) filterFreqEl.value = staged.filter?.freq ?? 20;
    if (filterQEl) filterQEl.value = staged.filter?.q ?? 1;
  }

  function populateArg(staged) {
    if (!staged) return;
    if (argEnableEl) argEnableEl.value = String(staged.arg?.enable ?? true);
    if (argAEl) argAEl.value = staged.arg?.a ?? 0;
    if (argBEl) argBEl.value = staged.arg?.b ?? 0;
  }

  function stageFilter(key, value) {
    runtime.stage((draft) => {
      draft.filter = draft.filter || {};
      draft.filter[key] = value;
      return draft;
    });
  }

  function stageArg(key, value) {
    runtime.stage((draft) => {
      draft.arg = draft.arg || {};
      draft.arg[key] = value;
      return draft;
    });
  }

  function renderLedGrid(staged) {
    if (!ledGrid) return;
    ledGrid.innerHTML = '';
    const leds = staged?.ledColors ?? [];
    const fragment = document.createDocumentFragment();
    leds.forEach((entry, index) => {
      const wrap = document.createElement('label');
      wrap.className = 'led-chip';
      wrap.textContent = `LED ${index + 1}`;
      const input = document.createElement('input');
      input.type = 'color';
      input.value = entry?.color ?? '#000000';
      input.addEventListener('input', runtime.createThrottle((event) => {
        runtime.stage((draft) => {
          draft.ledColors = draft.ledColors || [];
          if (!draft.ledColors[index]) draft.ledColors[index] = { color: '#000000' };
          draft.ledColors[index].color = event.target.value;
          return draft;
        });
      }));
      wrap.appendChild(input);
      fragment.appendChild(wrap);
    });
    ledGrid.appendChild(fragment);
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

  function renderDeviceMonitor(manifest) {
    if (!deviceMonitor) return;
    deviceMonitor.innerHTML = '';
    const entries = {
      Firmware: manifest?.fw_version || '—',
      'Git hash': manifest?.fw_git || '—',
      'Build time': manifest?.build_ts || '—',
      'Schema version': manifest?.schema_version || '—',
      'Free RAM': manifest?.free?.ram ? `${Math.round(manifest.free.ram / 1024)} KiB` : '—',
      'Free Flash': manifest?.free?.flash ? `${Math.round(manifest.free.flash / 1024)} KiB` : '—'
    };
    Object.entries(entries).forEach(([label, value]) => {
      const row = document.createElement('div');
      row.className = 'monitor-row';
      const term = document.createElement('span');
      term.className = 'monitor-label';
      term.textContent = label;
      const val = document.createElement('span');
      val.className = 'monitor-value';
      val.textContent = value;
      row.append(term, val);
      deviceMonitor.appendChild(row);
    });
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
    state.textContent = slot?.type ?? '—';
    const value = document.createElement('span');
    value.className = 'slot-value';
    value.textContent = slotState.telemetry?.slots?.[index] ?? '0';
    const toggle = document.createElement('button');
    toggle.type = 'button';
    toggle.className = 'takeover';
    toggle.textContent = slot?.takeover ? 'Takeover' : 'Guarded';
    toggle.onclick = () => {
      runtime.stage((draft) => {
        draft.slots[index].takeover = !draft.slots[index].takeover;
        return draft;
      });
      runtime.setPotGuard([index], !slot?.takeover);
    };
    el.append(label, state, value, toggle);
    el.onclick = () => selectSlot(index);
    el.setAttribute('role', 'button');
    el.classList.toggle('selected', index === slotState.selected);
  }

  function renderEfRow(el, index, item) {
    el.className = 'ef-row';
    el.innerHTML = '';
    const label = document.createElement('span');
    label.textContent = `EF ${index + 1}`;
    const input = document.createElement('input');
    input.type = 'number';
    input.min = 0;
    input.max = slotState.slots.length - 1;
    input.value = item?.slot ?? 0;
    input.addEventListener('change', () => {
      runtime.stage((draft) => {
        draft.efSlots = draft.efSlots || [];
        if (!draft.efSlots[index]) draft.efSlots[index] = { slot: 0 };
        draft.efSlots[index].slot = Number(input.value);
        return draft;
      });
    });
    el.append(label, input);
  }

  function paintTelemetry(frame) {
    if (!Array.isArray(frame.slots)) return;
    slotVirtualizer.updateTelemetry(frame.slots);
    frame.envelopes?.forEach((value, idx) => {
      const entry = envMeters[idx];
      if (!entry) return;
      entry.progress.value = value;
      entry.value.textContent = String(value);
    });
    if (frame.free) {
      renderDeviceMonitor({ ...runtime.getState().manifest, free: frame.free });
    }
    slotDetailValue.textContent = frame.slots[slotState.selected] ?? '—';
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

  class VirtualGrid {
    constructor(container, { columns, rowHeight, render }) {
      this.container = container;
      this.columns = columns;
      this.rowHeight = rowHeight;
      this.renderItem = render;
      this.data = [];
      this.pool = [];
      this.viewport = document.createElement('div');
      this.viewport.className = 'virtual-grid';
      if (this.container) {
        this.container.style.position = 'relative';
        this.container.style.overflowY = 'auto';
        this.container.appendChild(this.viewport);
        this.container.__resizeCallback = () => this.compute();
        sharedResizeObserver?.observe(this.container);
      }
      this.viewport.style.position = 'relative';
      this.viewport.style.width = '100%';
      this.container?.addEventListener('scroll', () => this.render());
    }

    setData(data) {
      this.data = data || [];
      this.compute();
    }

    compute() {
      if (!this.container) return;
      const visibleRows = Math.ceil(this.container.clientHeight / this.rowHeight) + 2;
      const needed = visibleRows * this.columns;
      while (this.pool.length < needed) {
        const el = document.createElement('div');
        el.className = 'virtual-item';
        el.style.position = 'absolute';
        this.viewport.appendChild(el);
        this.pool.push(el);
      }
      this.render();
    }

    render() {
      if (!this.container) return;
      const scrollTop = this.container.scrollTop;
      const firstRow = Math.max(0, Math.floor(scrollTop / this.rowHeight) - 1);
      const startIndex = firstRow * this.columns;
      this.viewport.style.height = `${Math.ceil(this.data.length / this.columns) * this.rowHeight}px`;
      this.pool.forEach((el, idx) => {
        const dataIndex = startIndex + idx;
        if (dataIndex >= this.data.length) {
          el.style.display = 'none';
          return;
        }
        el.style.display = '';
        const row = Math.floor(dataIndex / this.columns);
        const column = dataIndex % this.columns;
        el.style.width = `${100 / this.columns}%`;
        el.style.transform = `translate(${column * 100}%, ${row * this.rowHeight}px)`;
        el.style.height = `${this.rowHeight}px`;
        el.dataset.index = String(dataIndex);
        this.renderItem(el, dataIndex, this.data[dataIndex]);
      });
    }

    scrollToIndex(index) {
      if (!this.container) return;
      const row = Math.floor(index / this.columns);
      const target = row * this.rowHeight;
      this.container.scrollTo({ top: target, behavior: 'smooth' });
    }

    highlight(index) {
      this.pool.forEach((el) => {
        el.classList.toggle('selected', Number(el.dataset.index) === index);
      });
    }

    updateTelemetry(values) {
      this.pool.forEach((el) => {
        if (el.dataset.index === undefined) return;
        const idx = Number(el.dataset.index);
        const value = values[idx] ?? 0;
        el.dataset.value = value;
        const meter = el.querySelector('.slot-value');
        if (meter) meter.textContent = value;
      });
    }
  }

  class VirtualList {
    constructor(container, { itemHeight, render }) {
      this.container = container;
      this.itemHeight = itemHeight;
      this.renderItem = render;
      this.data = [];
      this.pool = [];
      this.viewport = document.createElement('div');
      this.viewport.className = 'virtual-list';
      if (this.container) {
        this.container.style.position = 'relative';
        this.container.style.overflowY = 'auto';
        this.container.appendChild(this.viewport);
        this.container.__resizeCallback = () => this.compute();
        sharedResizeObserver?.observe(this.container);
      }
      this.viewport.style.position = 'relative';
      this.viewport.style.width = '100%';
      this.container?.addEventListener('scroll', () => this.render());
    }

    setData(data) {
      this.data = data || [];
      this.compute();
    }

    compute() {
      if (!this.container) return;
      const visible = Math.ceil(this.container.clientHeight / this.itemHeight) + 2;
      while (this.pool.length < visible) {
        const el = document.createElement('div');
        el.className = 'virtual-row';
        el.style.position = 'absolute';
        this.viewport.appendChild(el);
        this.pool.push(el);
      }
      this.render();
    }

    render() {
      if (!this.container) return;
      const scrollTop = this.container.scrollTop;
      const first = Math.max(0, Math.floor(scrollTop / this.itemHeight) - 1);
      this.viewport.style.height = `${this.data.length * this.itemHeight}px`;
      this.pool.forEach((el, idx) => {
        const dataIndex = first + idx;
        if (dataIndex >= this.data.length) {
          el.style.display = 'none';
          return;
        }
        el.style.display = '';
        el.style.width = '100%';
        el.style.transform = `translateY(${dataIndex * this.itemHeight}px)`;
        this.renderItem(el, dataIndex, this.data[dataIndex]);
      });
    }
  }
});
