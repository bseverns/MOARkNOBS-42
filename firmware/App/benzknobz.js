window.addEventListener('DOMContentLoaded', () => {
  const localManifest = {
    ui_version: '2024.07.01',
    schemaVersion: '1.2.0',
    slotCount: 42,
    maxTableLengths: {
      ledColors: 16,
      efSlots: 6
    }
  };

  const migrations = {
    '1.1.0->1.2.0': (config) => {
      const next = deepClone(config);
      if (!next.ledColors || next.ledColors.length < localManifest.maxTableLengths.ledColors) {
        next.ledColors = Array.from({ length: localManifest.maxTableLengths.ledColors }, (_, idx) => ({
          color: config.ledColors?.[idx]?.color || '#000000'
        }));
      }
      return next;
    }
  };

  const TELEMETRY_INTERVAL = 1000 / 45;
  const OUTBOUND_SPACING = 24;

  let port, reader, writer;
  let remoteManifest = null;
  let schemaCache = null;
  let currentConfig = null;
  let stagedSnapshot = null;
  let pendingTelemetry = null;
  let telemetryTimer = null;
  let lastSendTime = 0;
  const outboundQueue = [];
  let sending = false;
  let dirty = false;
  let migrationAccepted = false;
  let hydrating = false;
  let useSimulator = false;
  let supportsAtomic = false;

  const encoder = new TextEncoder();
  const lineQueue = [];
  const lineWaiters = [];

  const root = document.documentElement;
  const pageShell = document.getElementById('page-shell');
  const statusEl = document.getElementById('status');
  const statusLabel = document.getElementById('status-label');
  const connectBtn = document.getElementById('connect');
  const saveBtn = document.getElementById('save');
  const connectCard = document.getElementById('connect-card');
  const stagePanel = document.getElementById('stage-panel');
  const livePanel = document.getElementById('live-panel');
  const slotContainer = document.getElementById('slots');
  const envContainer = document.getElementById('envelopes');
  const slotDetailIndex = document.getElementById('slot-detail-index');
  const slotDetailStatus = document.getElementById('slot-detail-status');
  const slotDetailType = document.getElementById('slot-detail-type');
  const slotDetailChannel = document.getElementById('slot-detail-channel');
  const slotDetailData = document.getElementById('slot-detail-data');
  const slotDetailEnvelope = document.getElementById('slot-detail-envelope');
  const slotDetailArg = document.getElementById('slot-detail-arg');
  const slotDetailValue = document.getElementById('slot-detail-value');
  const filterTypeEl = document.getElementById('filter-type');
  const filterFreqEl = document.getElementById('filter-freq');
  const filterQEl = document.getElementById('filter-q');
  const argEnableEl = document.getElementById('arg-enable');
  const argAEl = document.getElementById('arg-a');
  const argBEl = document.getElementById('arg-b');
  const logEl = document.getElementById('log');
  const efAssignmentGrid = document.getElementById('ef-assignment-card')?.querySelector('.ef-grid');
  const dirtyBadge = document.getElementById('dirty-badge');
  const connectionPill = document.getElementById('connection-pill');
  const potToggle = document.getElementById('pot-write-toggle');
  const exportPresetBtn = document.getElementById('export-preset');
  const importPresetBtn = document.getElementById('import-preset');
  const simulatorToggle = document.getElementById('simulator-toggle');
  const migrationDialog = document.getElementById('migration-dialog');
  const migrationPreview = document.getElementById('migration-preview');
  const migrationExport = document.getElementById('migration-export');
  const migrationApply = document.getElementById('migration-apply');
  const migrationCancel = document.getElementById('migration-cancel');
  const deviceMonitor = document.getElementById('device-monitor');

  const slotEls = [];
  const envEls = [];
  const controlRegistry = new Map();
  const pendingFieldUpdates = new WeakMap();
  const simulator = createSimulator();

  let efSelectRows = [];
  let selectedSlotIndex = 0;
  let telemetrySlotIndex = null;
  let latestArgMethod = null;

  const applyPageScale = () => {
    if (!pageShell || !root) return;
    root.style.setProperty('--page-scale', '1');
    pageShell.style.marginLeft = '';
    pageShell.style.marginRight = '';

    const totalHeight = pageShell.scrollHeight;
    const totalWidth = pageShell.scrollWidth;
    if (!Number.isFinite(totalHeight) || totalHeight <= 0) return;

    const bodyStyles = window.getComputedStyle(document.body);
    const padTop = parseFloat(bodyStyles.paddingTop) || 0;
    const padBottom = parseFloat(bodyStyles.paddingBottom) || 0;
    const padLeft = parseFloat(bodyStyles.paddingLeft) || 0;
    const padRight = parseFloat(bodyStyles.paddingRight) || 0;
    const availableHeight = Math.max(0, window.innerHeight - padTop - padBottom);
    if (availableHeight <= 0) return;

    const scale = Math.min(1, availableHeight / totalHeight);
    root.style.setProperty('--page-scale', scale.toFixed(4));

    const availableWidth = Math.max(0, window.innerWidth - padLeft - padRight);
    const scaledWidth = totalWidth * scale;
    const horizontalMargin = Math.max(0, (availableWidth - scaledWidth) / 2);
    pageShell.style.marginLeft = `${horizontalMargin}px`;
    pageShell.style.marginRight = `${horizontalMargin}px`;
  };

  const syncLayoutHeights = () => {
    let connectHeight = 0;
    if (connectCard) {
      const { height } = connectCard.getBoundingClientRect();
      if (Number.isFinite(height)) {
        connectHeight = height;
      }
    }

    if (stagePanel) {
      if (connectHeight > 0) {
        const stageTarget = Math.max(0, Math.round(connectHeight * 1.1));
        stagePanel.style.minHeight = stageTarget ? `${stageTarget}px` : '';
      } else {
        stagePanel.style.minHeight = '';
      }
    }

    if (livePanel) {
      if (connectHeight > 0) {
        const liveTarget = Math.max(0, Math.round(connectHeight * 1.1));
        livePanel.style.minHeight = liveTarget ? `${liveTarget}px` : '';
      } else {
        livePanel.style.minHeight = '';
      }
    }

    const referenceHeight = (() => {
      if (livePanel) {
        const { height } = livePanel.getBoundingClientRect();
        if (Number.isFinite(height) && height > 0) return height;
      }
      if (stagePanel) {
        const { height } = stagePanel.getBoundingClientRect();
        if (Number.isFinite(height) && height > 0) return height;
      }
      if (connectHeight > 0) return connectHeight;
      return 0;
    })();

    if (slotContainer) {
      if (referenceHeight > 0) {
        const slotTarget = Math.max(0, Math.round(referenceHeight * 0.9));
        slotContainer.style.minHeight = slotTarget ? `${slotTarget}px` : '';
      } else {
        slotContainer.style.minHeight = '';
      }
    }

    applyPageScale();
  };

  const scheduleLayoutSync = () => {
    if (typeof requestAnimationFrame === 'function') {
      requestAnimationFrame(syncLayoutHeights);
    } else {
      syncLayoutHeights();
    }
  };

  if (typeof ResizeObserver === 'function') {
    const observer = new ResizeObserver(() => scheduleLayoutSync());
    if (pageShell) observer.observe(pageShell);
    if (connectCard) observer.observe(connectCard);
    if (livePanel) observer.observe(livePanel);
    if (stagePanel) observer.observe(stagePanel);
  }

  window.addEventListener('resize', scheduleLayoutSync);

  initializeMeters(slotContainer, slotEls, localManifest.slotCount, 'Slot');
  initializeMeters(envContainer, envEls, localManifest.maxTableLengths.efSlots, 'EF');
  selectSlot(0);
  scheduleLayoutSync();

  initializeUI();

  function initializeUI() {
    updateConnectionStage('disconnected');
    markDirty(false);
    exportPresetBtn.disabled = true;
    importPresetBtn.disabled = true;
    if (location.protocol === 'file:') {
      setStatus('Serve this page with "python3 -m http.server" so WebSerial will cooperate.');
    }
    connectBtn?.addEventListener('click', async () => {
      try {
        await connect();
      } catch (_) {
        /* already surfaced */
      }
    });
    saveBtn?.addEventListener('click', () => saveConfig());
    exportPresetBtn?.addEventListener('click', () => exportPreset());
    importPresetBtn?.addEventListener('click', () => triggerPresetImport());
    simulatorToggle?.addEventListener('click', () => toggleSimulator());
    migrationExport?.addEventListener('click', () => exportPreset(true));
    migrationApply?.addEventListener('click', () => {
      migrationAccepted = true;
      migrationDialog.close('apply');
    });
    migrationCancel?.addEventListener('click', () => {
      migrationAccepted = false;
      migrationDialog.close('cancel');
    });
    potToggle?.addEventListener('change', () => {
      setStatus(potToggle.checked ? 'Pot writes enabled. Hold onto your stage mix.' : 'Pot writes gated. No surprise jumps.', potToggle.checked ? 'warn' : 'ok');
    });

    setInterval(async () => {
      try {
        if (port || useSimulator) return;
        if (!navigator.serial || typeof navigator.serial.getPorts !== 'function') return;
        const ports = await navigator.serial.getPorts();
        if (ports.length) {
          setStatus('Device is back. Smash Connect?', 'warn');
        }
      } catch (err) {
        console.debug('reconnect poll', err);
      }
    }, 5000);
  }

  function toggleSimulator() {
    useSimulator = !useSimulator;
    simulatorToggle.textContent = useSimulator ? 'Stop simulator' : 'Start simulator';
    simulatorToggle.classList.toggle('active', useSimulator);
    if (useSimulator) {
      setStatus('Simulator armed. Connect to jam without hardware.', 'ok');
    } else {
      setStatus('Simulator off. Waiting for the real synth.', 'warn');
    }
  }

  async function connect() {
    try {
      connectBtn.disabled = true;
      updateConnectionStage('handshake');
      setStatus('Summoning the synth…', 'warn', 'handshake');
      port = useSimulator ? await simulator.requestPort() : await navigator.serial.requestPort();
      if (!port) throw new Error('No serial port granted');
      await port.open({ baudRate: 115200 });

      const textDecoder = new TextDecoderStream();
      port.readable.pipeTo(textDecoder.writable);
      reader = textDecoder.readable.getReader();
      writer = port.writable.getWriter();

      readLoop().catch((err) => console.error('readLoop', err));

      await performHandshake();
      await syncSchemaAndConfig();
      exportPresetBtn.disabled = false;
      importPresetBtn.disabled = false;
      setStatus('Synced. Stage edits, then Apply.', 'ok', 'live');
    } catch (err) {
      console.error(err);
      setStatus(`Connection failed: ${err.message || err}`, 'err', 'disconnected');
      connectBtn.disabled = false;
      throw err;
    }
  }

  async function performHandshake() {
    setStatus('Negotiating manifest…', 'warn', 'handshake');
    let raw;
    try {
      raw = await send('GET_MANIFEST');
    } catch (err) {
      try {
        raw = await send('HELLO');
      } catch (_) {
        raw = JSON.stringify({
          fw_version: 'unknown',
          schema_version: localManifest.schemaVersion,
          slot_count: localManifest.slotCount,
          max_table_lengths: localManifest.maxTableLengths,
          capabilities: { atomicApply: false }
        });
      }
    }
    remoteManifest = normalizeManifest(raw);
    supportsAtomic = !!remoteManifest.capabilities?.atomicApply;
    updateDeviceMonitor(remoteManifest);
    const diffs = diffManifest(remoteManifest, localManifest);
    if (diffs.length) {
      migrationPreview.textContent = diffs.join('\n');
      migrationDialog.showModal();
      const decision = await new Promise((resolve) => {
        migrationDialog.addEventListener('close', () => resolve(migrationDialog.returnValue), { once: true });
      });
      if (decision !== 'apply') {
        throw new Error('Schema mismatch — migration aborted by user');
      }
    }
    migrationAccepted = true;
  }

  async function syncSchemaAndConfig() {
    schemaCache = await loadSchema();
    let config = await loadConfig();
    currentConfig = deepClone(config);
    hydrateForm(schemaCache, config);
    await loadFilter();
    await loadARGPair();
    markDirty(false);
    updateSaveState();
  }

  function getLine() {
    return lineQueue.length
    ? Promise.resolve(lineQueue.shift())
    : new Promise((resolve) => lineWaiters.push(resolve));
  }

  function send(cmd, options = {}) {
    if (!writer) return Promise.reject(new Error('Writer not ready'));
    const expectsReply = options.expectsReply !== false;
    return new Promise((resolve, reject) => {
      outboundQueue.push({ cmd, expectsReply, resolve, reject });
      pumpQueue();
    });
  }

  async function pumpQueue() {
    if (sending) return;
    sending = true;
    while (outboundQueue.length && writer) {
      const job = outboundQueue.shift();
      try {
        const now = performance.now();
        const wait = Math.max(0, OUTBOUND_SPACING - (now - lastSendTime));
        if (wait) await sleep(wait);
        await writer.write(encoder.encode(job.cmd + '\n'));
        lastSendTime = performance.now();
        const reply = job.expectsReply ? await getLine() : undefined;
        job.resolve(reply);
      } catch (err) {
        job.reject(err);
      }
    }
    sending = false;
  }

  async function readLoop() {
    let buf = '';
    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        buf += value;
        let idx;
        while ((idx = buf.indexOf('\n')) >= 0) {
          const line = buf.slice(0, idx).trim();
          buf = buf.slice(idx + 1);
          if (!line) continue;
          if (logEl) logEl.textContent += line + '\n';
          if (handleTelemetry(line)) continue;
          if (lineWaiters.length) lineWaiters.shift()(line);
          else lineQueue.push(line);
        }
      }
    } finally {
      tearDownConnection();
    }
  }

  function handleTelemetry(line) {
    try {
      const msg = JSON.parse(line);
      if (msg && (msg.slots || msg.envelopes || msg.argMethod !== undefined)) {
        pendingTelemetry = msg;
        scheduleTelemetryFlush();
        return true;
      }
    } catch (_) {
      /* ignore */
    }
    return false;
  }

  function scheduleTelemetryFlush() {
    if (telemetryTimer) return;
    telemetryTimer = setTimeout(() => {
      telemetryTimer = null;
      if (!pendingTelemetry) return;
      drawState(pendingTelemetry);
      pendingTelemetry = null;
    }, TELEMETRY_INTERVAL);
  }

  function drawState(msg) {
    let detailsDirty = false;
    if (Array.isArray(msg.slots)) {
      ensureMeterCount(slotContainer, slotEls, msg.slots.length, 'Slot');
      msg.slots.forEach((v, i) => {
        const meter = slotEls[i];
        if (!meter) return;
        meter.p.value = v;
        meter.s.textContent = v;
        meter.value = v;
        flash(meter.s);
      });
      detailsDirty = true;
    }
    if (Array.isArray(msg.envelopes)) {
      ensureMeterCount(envContainer, envEls, msg.envelopes.length, 'EF');
      msg.envelopes.forEach((v, i) => {
        const meter = envEls[i];
        if (!meter) return;
        meter.p.value = v;
        meter.s.textContent = v;
        flash(meter.s);
      });
    }
    if (typeof msg.currentSlot === 'number') {
      telemetrySlotIndex = Math.max(0, Math.min(slotEls.length - 1, msg.currentSlot));
      slotEls.forEach((entry, idx) => {
        entry.wrap.classList.toggle('active', idx === telemetrySlotIndex);
      });
      if (telemetrySlotIndex === selectedSlotIndex) {
        detailsDirty = true;
      }
    }
    if (Array.isArray(msg.efStatus) && efSelectRows.length) {
      efSelectRows.forEach((entry, idx) => {
        entry.row.classList.toggle('active', !!msg.efStatus[idx]);
      });
    }
    if (msg.argMethod !== undefined) {
      latestArgMethod = msg.argMethod;
      detailsDirty = true;
    }
    if (detailsDirty) updateSlotDetails();
  }

  function updateSlotDetails() {
    const source = stagedSnapshot?.legacy || currentConfig || {};
    const slots = Array.isArray(source.slots) ? source.slots : [];
    const slot = slots[selectedSlotIndex];
    const hasSlot = slot && typeof slot === 'object';
    const safeSlot = hasSlot ? slot : {};
    if (slotDetailIndex) {
      const label = Number.isFinite(selectedSlotIndex)
        ? `Slot ${String(selectedSlotIndex + 1).padStart(2, '0')}`
        : '—';
      slotDetailIndex.textContent = label;
    }
    if (slotDetailStatus) {
      if (!hasSlot) slotDetailStatus.textContent = '—';
      else if (telemetrySlotIndex === selectedSlotIndex) slotDetailStatus.textContent = 'Live';
      else if (safeSlot.active === false) slotDetailStatus.textContent = 'Muted';
      else slotDetailStatus.textContent = safeSlot.active ? 'Active' : 'Armed';
    }
    if (slotDetailType) slotDetailType.textContent = safeSlot.type ?? '—';
    if (slotDetailChannel) {
      slotDetailChannel.textContent = safeSlot.midiChannel !== undefined ? safeSlot.midiChannel : '—';
    }
    if (slotDetailData) {
      slotDetailData.textContent = safeSlot.data1 !== undefined ? safeSlot.data1 : '—';
    }
    if (slotDetailEnvelope) {
      slotDetailEnvelope.textContent = safeSlot.efIndex !== undefined ? `EF ${safeSlot.efIndex + 1}` : '—';
    }
    if (slotDetailArg) {
      const method = extractArgMethod(source) || latestArgMethod;
      slotDetailArg.textContent = method ? String(method).toUpperCase() : '—';
    }
    if (slotDetailValue) {
      const meter = slotEls[selectedSlotIndex];
      const value = meter?.value;
      slotDetailValue.textContent = value !== undefined ? value : '—';
    }
  }

  function selectSlot(index) {
    if (!slotEls.length) {
      selectedSlotIndex = 0;
      updateSlotDetails();
      return;
    }
    const clamped = Math.max(0, Math.min(slotEls.length - 1, Number(index) || 0));
    selectedSlotIndex = clamped;
    slotEls.forEach((entry, idx) => {
      if (!entry?.wrap) return;
      const isSelected = idx === clamped;
      entry.wrap.classList.toggle('selected', isSelected);
      entry.wrap.setAttribute('aria-pressed', isSelected ? 'true' : 'false');
    });
    updateSlotDetails();
  }

  function extractArgMethod(source) {
    if (!source) return undefined;
    const argGroup = source.arg;
    if (Array.isArray(argGroup)) return argGroup[0]?.method;
    if (argGroup && typeof argGroup === 'object') return argGroup.method;
    return undefined;
  }

  function flash(el) {
    if (!el) return;
    el.classList.remove('flash');
    void el.offsetWidth;
    el.classList.add('flash');
  }

  async function loadSchema() {
    setStatus('Loading schema from the rig…', 'warn');
    try {
      const raw = await send('GET_SCHEMA');
      setStatus('Schema loaded. The knobs know their shapes.', 'ok');
      return JSON.parse(raw);
    } catch (err) {
      console.warn('Device schema failed, falling back to local file', err);
      try {
        const res = await fetch('config_schema.json');
        const json = await res.json();
        setStatus('Schema loaded (local fallback).', 'warn');
        return json;
      } catch (fetchErr) {
        setStatus(`Schema error: ${fetchErr.message || fetchErr}`, 'err');
        throw fetchErr;
      }
    }
  }

  async function loadConfig() {
    setStatus('Pulling current scene…', 'warn');
    const raw = await send('GET_ALL');
    let json = JSON.parse(raw);
    if (migrationAccepted && remoteManifest?.schema_version && remoteManifest.schema_version !== localManifest.schemaVersion) {
      const migrated = runMigrations(json, remoteManifest.schema_version, localManifest.schemaVersion);
      migrationPreview.textContent = formatDiff(json, migrated);
      json = migrated;
    }
    setStatus('Config synced. Tweak with intent.', 'ok');
    return json;
  }

  async function loadFilter() {
    try {
      const raw = await send('GET_FILTER');
      const [type, freq, q] = raw.split(',');
      if (filterTypeEl) filterTypeEl.value = type;
      if (filterFreqEl) filterFreqEl.value = freq;
      if (filterQEl) filterQEl.value = q;
    } catch (err) {
      setStatus(`Filter load fail: ${err.message || err}`, 'err');
    }
  }

  async function loadARGPair() {
    try {
      const raw = await send('GET_ARGPAIR');
      const [en, a, b] = raw.split(',');
      if (argEnableEl) argEnableEl.value = en;
      if (argAEl) argAEl.value = a;
      if (argBEl) argBEl.value = b;
    } catch (err) {
      setStatus(`ARG load fail: ${err.message || err}`, 'err');
    }
  }

  function hydrateForm(schema, values) {
    const container = document.getElementById('form');
    if (!container) return;
    hydrating = true;
    container.innerHTML = '';
    const ledContainer = document.getElementById('led-settings');
    if (ledContainer) ledContainer.innerHTML = '<legend>LED Colors</legend>';
    if (efAssignmentGrid) efAssignmentGrid.innerHTML = '';
    efSelectRows = [];
    controlRegistry.clear();

    for (const field of schema) {
      if (field.key === 'ledColors' && ledContainer) {
        const count = field.count || localManifest.maxTableLengths.ledColors || 0;
        for (let i = 0; i < count; i++) {
          const val = values[field.key]?.[i]?.color || '#000000';
          const inp = document.createElement('input');
          inp.type = 'color';
          inp.name = `ledColors[${i}].color`;
          inp.value = val;
          const lab = document.createElement('label');
          lab.textContent = `LED ${i + 1}`;
          lab.appendChild(inp);
          ledContainer.appendChild(lab);
          registerControl(inp, {
            id: `led-${i}`,
            label: `LED ${i + 1}`,
            key: inp.name,
            type: 'color',
            control: 'encoder',
            docRef: 'Docs › LED table'
          });
        }
        continue;
      }

      if (field.key === 'efSlots' && efAssignmentGrid) {
        const totalSlots = values.slots?.length || localManifest.slotCount;
        const slotLabels = Array.from({ length: totalSlots }, (_, idx) => {
          const slot = values.slots?.[idx] || {};
          const num = idx.toString().padStart(2, '0');
          const type = slot.type ?? 'Slot';
          const chan = slot.midiChannel !== undefined ? ` ch${slot.midiChannel}` : '';
          const data = slot.data1 !== undefined ? ` #${slot.data1}` : '';
          return `Slot ${num} — ${type}${chan}${data}`;
        });
        const count = field.count || localManifest.maxTableLengths.efSlots;
        for (let i = 0; i < count; i++) {
          const lab = document.createElement('label');
          lab.className = 'ef-select';
          lab.innerHTML = `<span>EF ${i + 1}</span>`;
          const sel = document.createElement('select');
          sel.name = `efSlots[${i}].slot`;
          const current = values[field.key]?.[i]?.slot ?? 0;
          for (let idx = 0; idx < totalSlots; idx++) {
            const opt = document.createElement('option');
            opt.value = idx;
            opt.textContent = slotLabels[idx];
            if (idx === current) opt.selected = true;
            sel.appendChild(opt);
          }
          lab.appendChild(sel);
          efAssignmentGrid.appendChild(lab);
          efSelectRows.push({ row: lab, select: sel });
          registerControl(sel, {
            id: `ef-${i}`,
            label: `Envelope follower ${i + 1}`,
            key: sel.name,
            type: 'select',
            control: 'encoder',
            docRef: 'Docs › Envelope followers'
          });
        }
        continue;
      }

      if (field.type === 'group') {
        const groupEl = document.createElement('fieldset');
        groupEl.innerHTML = `<legend>${field.label}</legend>`;
        const count = field.count || 1;
        for (let i = 0; i < count; i++) {
          const sub = document.createElement('div');
          for (const f of field.fields) {
            const key = `${field.key}[${i}].${f.subkey}`;
            const val = values[field.key]?.[i]?.[f.subkey];
            const labelText = `${f.label}${count > 1 ? ` ${i}` : ''}`;
            const lab = document.createElement('label');
            lab.textContent = labelText;
            let control;
            if (f.type === 'select') {
              control = document.createElement('select');
              control.name = key;
              for (const opt of f.options || []) {
                const option = document.createElement('option');
                option.value = opt;
                option.textContent = opt;
                if (opt === val) option.selected = true;
                control.appendChild(option);
              }
            } else if (f.type === 'boolean') {
              control = document.createElement('input');
              control.type = 'checkbox';
              control.name = key;
              if (val) control.checked = true;
            } else {
              control = document.createElement('input');
              control.type = f.type || 'text';
              control.name = key;
              if (val !== undefined) control.value = val;
              if (f.min !== undefined) control.min = f.min;
              if (f.max !== undefined) control.max = f.max;
              if (f.step !== undefined) control.step = f.step;
            }
            lab.appendChild(control);
            sub.appendChild(lab);
            registerControl(control, {
              id: `${field.key}-${i}-${f.subkey}`,
              label: labelText,
              key,
              type: control.type,
              control: f.control || (control.type === 'number' ? 'encoder' : 'encoder'),
              docRef: f.docRef
            });
          }
          groupEl.appendChild(sub);
        }
        container.appendChild(groupEl);
        continue;
      }

      let control;
      const lab = document.createElement('label');
      lab.textContent = field.label;
      if (field.type === 'select') {
        control = document.createElement('select');
        control.name = field.key;
        for (const opt of field.options || []) {
          const option = document.createElement('option');
          option.value = opt;
          option.textContent = opt;
          if (opt === values[field.key]) option.selected = true;
          control.appendChild(option);
        }
      } else if (field.type === 'boolean') {
        control = document.createElement('input');
        control.type = 'checkbox';
        control.name = field.key;
        if (values[field.key]) control.checked = true;
      } else {
        control = document.createElement('input');
        control.type = field.type || 'text';
        control.name = field.key;
        if (values[field.key] !== undefined) control.value = values[field.key];
        if (field.min !== undefined) control.min = field.min;
        if (field.max !== undefined) control.max = field.max;
        if (field.step !== undefined) control.step = field.step;
      }
      lab.appendChild(control);
      container.appendChild(lab);
      registerControl(control, {
        id: field.id || field.key,
        label: field.label,
        key: field.key,
        type: control.type,
        control: field.control || (control.type === 'number' ? 'encoder' : 'encoder'),
        docRef: field.docRef
      });
    }

    registerControl(filterTypeEl, {
      id: 'filter-type',
      label: 'Filter Type',
      key: 'filter.type',
      type: 'select',
      control: 'encoder',
      docRef: 'Docs › Filter curves'
    });
    registerControl(filterFreqEl, {
      id: 'filter-freq',
      label: 'Filter Frequency',
      key: 'filter.freq',
      type: 'number',
      control: filterFreqEl?.dataset.control === 'pot' ? 'pot' : 'encoder',
      docRef: 'Docs › Filter ranges'
    });
    registerControl(filterQEl, {
      id: 'filter-q',
      label: 'Filter Q',
      key: 'filter.q',
      type: 'number',
      control: filterQEl?.dataset.control === 'encoder' ? 'encoder' : 'encoder',
      docRef: 'Docs › Filter ranges'
    });
    registerControl(argEnableEl, {
      id: 'arg-enable',
      label: 'ARG Enable',
      key: 'arg.enable',
      type: 'select',
      control: 'encoder',
      docRef: 'Docs › ARG'
    });
    registerControl(argAEl, {
      id: 'arg-a',
      label: 'ARG A',
      key: 'arg.a',
      type: 'number',
      control: 'encoder',
      docRef: 'Docs › ARG'
    });
    registerControl(argBEl, {
      id: 'arg-b',
      label: 'ARG B',
      key: 'arg.b',
      type: 'number',
      control: 'encoder',
      docRef: 'Docs › ARG'
    });

    hydrating = false;
    stagedSnapshot = collectFormData();
    latestArgMethod = extractArgMethod(stagedSnapshot.legacy) || latestArgMethod;
    updateSlotDetails();
  }

  function registerControl(control, meta = {}) {
    if (!control || control.dataset.bound === 'true') return;
    const controlId = meta.id || control.dataset.controlId || control.name || `control-${controlRegistry.size}`;
    control.dataset.controlId = controlId;
    control.dataset.bound = 'true';
    const record = {
      id: controlId,
      label: meta.label || control.getAttribute('aria-label') || controlId,
      key: meta.key || control.name || controlId,
      type: meta.type || control.type,
      control: meta.control || control.dataset.control || (control.type === 'number' ? 'encoder' : 'encoder'),
      docRef: meta.docRef,
      element: control
    };
    controlRegistry.set(controlId, record);
    control.addEventListener('input', () => handleFieldChange(record), { passive: true });
    control.addEventListener('change', () => handleFieldChange(record));
  }

  function handleFieldChange(record) {
    if (hydrating) return;
    runInlineValidation(record);
    if (record.control === 'pot' && potToggle && !potToggle.checked) {
      return;
    }
    if (pendingFieldUpdates.has(record)) {
      clearTimeout(pendingFieldUpdates.get(record));
    }
    const wait = record.control === 'encoder' ? 16 : 32;
    pendingFieldUpdates.set(record, setTimeout(() => {
      pendingFieldUpdates.delete(record);
      stagedSnapshot = collectFormData();
      latestArgMethod = extractArgMethod(stagedSnapshot.legacy) || latestArgMethod;
      markDirty(true);
      updateSlotDetails();
    }, wait));
  }

  function runInlineValidation(record) {
    const el = record.element;
    if (!el || typeof el.checkValidity !== 'function') return;
    if (el.checkValidity()) {
      el.setCustomValidity('');
      el.removeAttribute('aria-invalid');
    } else {
      const docRef = record.docRef ? ` — ${record.docRef}` : ' — see docs tables.';
      el.setCustomValidity(`Value outside supported range${docRef}`);
      el.setAttribute('aria-invalid', 'true');
      el.reportValidity();
    }
  }

  function collectFormData() {
    const legacy = {};
    const valuesById = {};
    controlRegistry.forEach((record) => {
      const el = record.element;
      if (!el) return;
      const raw = el.type === 'checkbox' ? el.checked : el.value;
      const value = el.type === 'checkbox' ? raw : (raw === '' || isNaN(raw) ? raw : +raw);
      valuesById[record.id] = value;
      if (el.name) assignPath(legacy, el.name, value);
    });
    const filter = {
      type: filterTypeEl?.value ?? '0',
      freq: filterFreqEl?.value ?? '0',
      q: filterQEl?.value ?? '0'
    };
    const method = extractArgMethod(legacy) || latestArgMethod || 'PLUS';
    const arg = {
      enable: argEnableEl?.value ?? '0',
      method,
      a: argAEl?.value ?? '0',
      b: argBEl?.value ?? '0'
    };
    return { legacy, valuesById, filter, arg };
  }

  function assignPath(target, path, value) {
    const m = path.match(/([^.]+)\[(\d+)\]\.(.+)/);
    if (m) {
      const [, group, idx, sub] = m;
      target[group] = target[group] || [];
      target[group][+idx] = target[group][+idx] || {};
      target[group][+idx][sub] = value;
    } else {
      target[path] = value;
    }
  }

  function markDirty(state) {
    dirty = !!state;
    if (dirtyBadge) dirtyBadge.style.display = dirty ? 'inline-flex' : 'none';
    updateConnectionStage(dirty ? 'dirty' : (writer ? 'live' : 'disconnected'));
    updateSaveState();
    scheduleLayoutSync();
  }

  function updateSaveState() {
    if (!saveBtn) return;
    saveBtn.disabled = !dirty || !writer;
  }

  function updateConnectionStage(stage) {
    if (!connectionPill) return;
    connectionPill.dataset.stage = stage;
    const map = {
      disconnected: 'Disconnected',
      handshake: 'Handshake',
      live: 'Live',
      dirty: 'Dirty'
    };
    connectionPill.textContent = map[stage] || stage;
  }

  function setStatus(msg, level = 'warn', stageHint) {
    if (!statusEl) return;
    const levels = {
      ok: { text: 'ONLINE', icon: 'M15.5 4.5 8.5 11.5 5 8' },
      warn: { text: 'NOTE', icon: 'M12 5v14M5 12h14' },
      err: { text: 'ALERT', icon: 'M12 8v4m0 4h.01' }
    };
    const state = levels[level] ? level : 'warn';
    statusEl.dataset.state = state;
    if (statusLabel) statusLabel.textContent = levels[state].text;
    const iconPath = statusEl.querySelector('path');
    if (iconPath) iconPath.setAttribute('d', levels[state].icon);
    const messageEl = statusEl.querySelector('.status-message');
    if (messageEl) messageEl.textContent = msg;
    if (stageHint) updateConnectionStage(stageHint);
    scheduleLayoutSync();
  }

  function updateDeviceMonitor(manifest) {
    if (!deviceMonitor) return;
    deviceMonitor.innerHTML = '';
    const entries = [
    ['Firmware', manifest.fw_version || '—'],
    ['Schema', manifest.schema_version || localManifest.schemaVersion],
    ['Slots', manifest.slot_count ?? localManifest.slotCount],
    ['Atomic Apply', supportsAtomic ? 'yes' : 'no']
  ];
    const tables = manifest.max_table_lengths || {};
    Object.keys(tables).forEach((key) => {
      entries.push([`Max ${key}`, tables[key]]);
    });
    entries.forEach(([label, value]) => {
      const item = document.createElement('div');
      item.className = 'monitor-item';
      const span = document.createElement('span');
      span.textContent = label;
      const strong = document.createElement('strong');
      strong.textContent = value;
      item.appendChild(span);
      item.appendChild(strong);
      deviceMonitor.appendChild(item);
    });
  }

  function exportPreset(forceRaw = false) {
    const snapshot = stagedSnapshot || collectFormData();
    const payload = {
      schema_version: localManifest.schemaVersion,
      captured_at: new Date().toISOString(),
      manifest: remoteManifest,
      values: forceRaw ? currentConfig : snapshot.legacy
    };
    const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = `moarknobz-preset-${Date.now()}.json`;
    link.click();
    URL.revokeObjectURL(url);
  }

  function triggerPresetImport() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = 'application/json';
    input.addEventListener('change', () => {
      if (input.files?.length) handlePresetFile(input.files[0]);
    }, { once: true });
    input.click();
  }

  async function handlePresetFile(file) {
    try {
      const text = await file.text();
      const json = JSON.parse(text);
      const from = json.schema_version || json.schemaVersion || remoteManifest?.schema_version || localManifest.schemaVersion;
      let payload = json.values || json.legacy || json.config || json;
      if (from && from !== localManifest.schemaVersion) {
        payload = runMigrations(payload, from, localManifest.schemaVersion);
      }
      if (schemaCache) {
        hydrateForm(schemaCache, payload);
      }
      currentConfig = deepClone(payload);
      stagedSnapshot = collectFormData();
      latestArgMethod = extractArgMethod(currentConfig) || latestArgMethod;
      updateSlotDetails();
      markDirty(true);
      setStatus('Preset imported. Review and apply.', 'warn', 'dirty');
    } catch (err) {
      setStatus(`Preset import failed: ${err.message || err}`, 'err');
    }
  }

  async function saveConfig() {
    try {
      const snapshot = collectFormData();
      setStatus('Packing staged edits…', 'warn', 'dirty');
      let applied = false;
      if (supportsAtomic) {
        applied = await applyAtomicPatch(snapshot);
        if (!applied) {
          setStatus('Atomic apply failed; falling back to legacy write.', 'warn', 'dirty');
        }
      }
      if (!applied) {
        await sendLegacyConfig(snapshot);
      }
      currentConfig = deepClone(snapshot.legacy);
      stagedSnapshot = snapshot;
      latestArgMethod = extractArgMethod(currentConfig) || latestArgMethod;
      updateSlotDetails();
      markDirty(false);
      setStatus('Settings landed. Go make noise.', 'ok', 'live');
    } catch (err) {
      console.error(err);
      setStatus(`Save failed: ${err.message || err}`, 'err', 'dirty');
    }
  }

  async function applyAtomicPatch(snapshot) {
    try {
      const basePayload = {
        manifest: {
          schema_version: localManifest.schemaVersion,
          slot_count: remoteManifest?.slot_count ?? localManifest.slotCount,
          fw_version: remoteManifest?.fw_version ?? 'unknown'
        },
        values: snapshot.valuesById,
        legacy: snapshot.legacy,
        filter: snapshot.filter,
        argPair: snapshot.arg,
        timestamp: Date.now()
      };
      const checksumValue = await checksum(JSON.stringify(basePayload));
      basePayload.checksum = checksumValue;
      const reply = await send(`APPLY_PATCH ${JSON.stringify(basePayload)}`);
      const ack = safeParseJSON(reply);
      if (!ack || ack.checksum !== checksumValue) {
        await send(`ROLLBACK ${checksumValue}`, { expectsReply: false }).catch(() => {});
        throw new Error('Checksum mismatch from device');
      }
      return true;
    } catch (err) {
      console.warn('Atomic apply failed', err);
      return false;
    }
  }

  async function sendLegacyConfig(snapshot) {
    await send(`SET_ALL ${JSON.stringify(snapshot.legacy)}`, { expectsReply: false });
    await send(`SET_FILTER ${snapshot.filter.type},${snapshot.filter.freq},${snapshot.filter.q}`);
    await send(`SET_ARGPAIR ${snapshot.arg.enable},${snapshot.arg.a},${snapshot.arg.b}`);
  }

  async function checksum(str) {
    try {
      if (crypto?.subtle) {
        const buffer = await crypto.subtle.digest('SHA-256', encoder.encode(str));
        return Array.from(new Uint8Array(buffer)).map((b) => b.toString(16).padStart(2, '0')).join('');
      }
    } catch (_) {
      /* fall through */
    }
    let hash = 2166136261 >>> 0;
    for (let i = 0; i < str.length; i++) {
      hash ^= str.charCodeAt(i);
      hash = Math.imul(hash, 16777619);
    }
    return hash.toString(16);
  }

  function safeParseJSON(line) {
    try {
      return JSON.parse(line);
    } catch (_) {
      return null;
    }
  }

  function runMigrations(config, fromVersion, toVersion) {
    if (fromVersion === toVersion) return deepClone(config);
    const key = `${fromVersion}->${toVersion}`;
    if (migrations[key]) {
      return migrations[key](config);
    }
    return deepClone(config);
  }

  function formatDiff(before, after) {
    const beforeLines = JSON.stringify(before, null, 2).split('\n').map((line) => `- ${line}`);
    const afterLines = JSON.stringify(after, null, 2).split('\n').map((line) => `+ ${line}`);
    return [...beforeLines, '---', ...afterLines].join('\n');
  }

  function normalizeManifest(raw) {
    let parsed;
    try {
      parsed = typeof raw === 'string' ? JSON.parse(raw) : raw;
    } catch (_) {
      parsed = {};
    }
    parsed = parsed || {};
    parsed.schema_version = parsed.schema_version || parsed.schemaVersion || localManifest.schemaVersion;
    parsed.slot_count = parsed.slot_count ?? parsed.slotCount ?? localManifest.slotCount;
    parsed.max_table_lengths = parsed.max_table_lengths || parsed.maxTableLengths || localManifest.maxTableLengths;
    parsed.capabilities = parsed.capabilities || {};
    return parsed;
  }

  function diffManifest(remote, local) {
    const diffs = [];
    if (remote.schema_version !== local.schemaVersion) {
      diffs.push(`Schema mismatch: device ${remote.schema_version} vs UI ${local.schemaVersion}`);
    }
    if ((remote.slot_count ?? local.slotCount) !== local.slotCount) {
      diffs.push(`Slot count mismatch: device ${remote.slot_count} vs UI ${local.slotCount}`);
    }
    const keys = new Set([
    ...Object.keys(local.maxTableLengths || {}),
    ...Object.keys(remote.max_table_lengths || {})
  ]);
    keys.forEach((key) => {
      const localVal = local.maxTableLengths?.[key];
      const remoteVal = remote.max_table_lengths?.[key];
      if (localVal !== undefined && remoteVal !== undefined && localVal !== remoteVal) {
        diffs.push(`Table ${key}: device ${remoteVal} vs UI ${localVal}`);
      }
    });
    return diffs;
  }

  function initializeMeters(container, store, count, labelPrefix) {
    if (!container) return;
    container.innerHTML = '';
    store.length = 0;
    for (let i = 0; i < count; i++) {
      const wrap = document.createElement('div');
      wrap.className = 'meter';
      wrap.dataset.index = String(i);
      if (labelPrefix === 'Slot') {
        wrap.classList.add('slot-button');
        wrap.setAttribute('role', 'button');
        wrap.setAttribute('tabindex', '0');
        wrap.setAttribute('aria-pressed', 'false');
        wrap.setAttribute('aria-label', `Slot ${i + 1}`);
        wrap.addEventListener('click', () => selectSlot(i));
        wrap.addEventListener('keydown', (event) => {
          let next = null;
          if (event.key === 'Enter' || event.key === ' ') {
            event.preventDefault();
            selectSlot(i);
            return;
          }
          if (event.key === 'ArrowRight') next = i + 1;
          else if (event.key === 'ArrowLeft') next = i - 1;
          else if (event.key === 'ArrowDown') next = i + 6;
          else if (event.key === 'ArrowUp') next = i - 6;
          if (next !== null) {
            event.preventDefault();
            selectSlot(next);
            const target = slotEls[Math.max(0, Math.min(slotEls.length - 1, next))]?.wrap;
            target?.focus?.();
          }
        });
      } else {
        wrap.setAttribute('role', 'listitem');
      }
      const p = document.createElement('progress');
      p.max = 127;
      p.value = 0;
      const s = document.createElement('span');
      s.textContent = '0';
      s.setAttribute('aria-label', `${labelPrefix} ${i + 1} value`);
      wrap.appendChild(p);
      wrap.appendChild(s);
      container.appendChild(wrap);
      store.push({ p, s, wrap, value: 0 });
    }
    if (labelPrefix === 'Slot') {
      selectSlot(Math.min(selectedSlotIndex, store.length ? store.length - 1 : 0));
    }
    scheduleLayoutSync();
  }

  function ensureMeterCount(container, store, count, labelPrefix) {
    if (!container) return;
    if (store.length === count) return;
    initializeMeters(container, store, count, labelPrefix);
  }

  function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }

  function deepClone(obj) {
    if (typeof structuredClone === 'function') return structuredClone(obj);
    return JSON.parse(JSON.stringify(obj));
  }

  function tearDownConnection() {
    try {
      reader?.releaseLock?.();
    } catch (_) {}
    try {
      writer?.releaseLock?.();
    } catch (_) {}
    port = null;
    reader = null;
    writer = null;
    updateConnectionStage('disconnected');
    setStatus('Port closed.', 'warn', 'disconnected');
    connectBtn.disabled = false;
    exportPresetBtn.disabled = true;
    importPresetBtn.disabled = true;
    markDirty(false);
  }

  function createSimulator() {
    const state = {
      manifest: {
        fw_version: 'sim-1.0.0',
        schema_version: localManifest.schemaVersion,
        slot_count: localManifest.slotCount,
        max_table_lengths: localManifest.maxTableLengths,
        capabilities: { atomicApply: true }
      },
      filter: { type: '0', freq: '440', q: '1.0' },
      arg: { enable: '1', method: 'PLUS', a: '1.0', b: '1.0' }
    };
    const decoder = new TextDecoder();
    let schemaPromise = null;
    let portInstance = null;
    let controller = null;

    async function getSchema() {
      if (!schemaPromise) {
        schemaPromise = fetch('config_schema.json')
        .then((res) => res.json())
        .catch(() => []);
      }
      return schemaPromise;
    }

    function ensureConfig() {
      if (!state.config) {
        state.config = {
          slots: Array.from({ length: state.manifest.slot_count }, (_, idx) => ({
            type: 'CC',
            midiChannel: 1,
            data1: idx
          })),
          ledColors: Array.from({ length: localManifest.maxTableLengths.ledColors }, () => ({ color: '#222222' })),
          efSlots: Array.from({ length: localManifest.maxTableLengths.efSlots }, (_, idx) => ({ slot: idx }))
        };
      }
      const config = state.config;
      config.arg = [
        {
          method: state.arg.method || 'PLUS',
          a: state.arg.a ?? '1.0',
          b: state.arg.b ?? '1.0'
        }
      ];
      return config;
    }

    class SimulatorPort {
      constructor() {
        this.readable = null;
        this.writable = null;
        this._telemetry = null;
      }

      async open() {
        ensureConfig();
        const self = this;
        this.readable = new ReadableStream({
          start(ctrl) {
            controller = ctrl;
            self._startTelemetry();
          },
          cancel() {
            self._stopTelemetry();
          }
        });
        this.writable = new WritableStream({
          async write(chunk) {
            const text = typeof chunk === 'string' ? chunk : decoder.decode(chunk);
            const commands = text.split('\n').map((line) => line.trim()).filter(Boolean);
            for (const line of commands) {
              await processCommand(line);
            }
          }
        });
      }

      async close() {
        this._stopTelemetry();
        controller?.close?.();
        controller = null;
      }

      _startTelemetry() {
        this._stopTelemetry();
        this._telemetry = setInterval(() => {
          const payload = {
            slots: Array.from({ length: state.manifest.slot_count }, () => Math.floor(Math.random() * 127)),
            envelopes: Array.from({ length: localManifest.maxTableLengths.efSlots }, () => Math.floor(Math.random() * 127)),
            efStatus: Array.from({ length: localManifest.maxTableLengths.efSlots }, (_, idx) => (idx % 2)),
            argMethod: state.arg.method || 'SIM'
          };
          respond(JSON.stringify(payload));
        }, 1000 / 30);
      }

      _stopTelemetry() {
        if (this._telemetry) clearInterval(this._telemetry);
        this._telemetry = null;
      }
    }

    function respond(line) {
      if (!controller) return;
      controller.enqueue(encoder.encode(line + '\n'));
    }

    async function processCommand(line) {
      ensureConfig();
      if (line.startsWith('GET_MANIFEST') || line === 'HELLO') {
        respond(JSON.stringify(state.manifest));
        return;
      }
      if (line.startsWith('GET_SCHEMA')) {
        const schema = await getSchema();
        respond(JSON.stringify(schema));
        return;
      }
      if (line.startsWith('GET_ALL')) {
        respond(JSON.stringify(ensureConfig()));
        return;
      }
      if (line.startsWith('GET_FILTER')) {
        const { type, freq, q } = state.filter;
        respond(`${type},${freq},${q}`);
        return;
      }
      if (line.startsWith('GET_ARGPAIR')) {
        const { enable, a, b } = state.arg;
        respond(`${enable},${a},${b}`);
        return;
      }
      if (line.startsWith('SET_ALL')) {
        const json = line.slice('SET_ALL'.length).trim();
        if (json) {
          try {
            state.config = JSON.parse(json);
          } catch (_) {}
        }
        return;
      }
      if (line.startsWith('SET_FILTER')) {
        const payload = line.replace('SET_FILTER', '').trim();
        const [type, freq, q] = payload.split(',');
        state.filter = { type, freq, q };
        respond(JSON.stringify({ ok: true }));
        return;
      }
      if (line.startsWith('SET_ARGPAIR')) {
        const payload = line.replace('SET_ARGPAIR', '').trim();
        const [enable, a, b] = payload.split(',');
        state.arg = { ...state.arg, enable, a, b };
        respond(JSON.stringify({ ok: true }));
        return;
      }
      if (line.startsWith('APPLY_PATCH')) {
        const json = line.slice('APPLY_PATCH'.length).trim();
        try {
          const payload = JSON.parse(json);
          const expected = await checksum(JSON.stringify({
            manifest: payload.manifest,
            values: payload.values,
            legacy: payload.legacy,
            filter: payload.filter,
            argPair: payload.argPair,
            timestamp: payload.timestamp
          }));
          if (payload.checksum !== expected) {
            respond(JSON.stringify({ checksum: 'mismatch' }));
            return;
          }
          state.config = deepClone(payload.legacy);
          state.filter = payload.filter;
          state.arg = payload.argPair;
          respond(JSON.stringify({ checksum: payload.checksum }));
        } catch (err) {
          respond(JSON.stringify({ error: err.message }));
        }
        return;
      }
      if (line.startsWith('ROLLBACK')) {
        respond(JSON.stringify({ rolledBack: true }));
      }
    }

    return {
      async requestPort() {
        if (!portInstance) {
          portInstance = new SimulatorPort();
        }
        return portInstance;
      }
    };
  }
});
