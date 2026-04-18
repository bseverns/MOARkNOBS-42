export function createSlotEditorPanel({
  runtime,
  localManifest,
  slotState,
  formContainer,
  detailElements = {},
  glossary = {},
  slotTypeNames = [],
  efFilterNames = [],
  argMethodNames = [],
  formatArgMethodLabel,
  describeArgMethod,
  getUiMode = () => 'basic',
  getEditorTab = () => 'mapping'
} = {}) {
  const {
    slotDetailIndex = null,
    slotDetailStatus = null,
    slotDetailType = null,
    slotDetailChannel = null,
    slotDetailData = null,
    slotDetailEfIndex = null,
    slotDetailEfFilter = null,
    slotDetailEfTuning = null,
    slotDetailEfDynamics = null,
    slotDetailEfBaseline = null,
    slotDetailArg = null,
    slotDetailArgSources = null,
    slotDetailValue = null
  } = detailElements;

  // Fill the slot detail card from the selected slot plus latest telemetry.
  function populateDetail() {
    const slot = slotState.slots[slotState.selected];
    const telemetry = slotState.telemetry || {};
    if (slotDetailIndex)
      slotDetailIndex.textContent =
        slotState.selected !== undefined
          ? `Slot ${String(slotState.selected + 1).padStart(2, '0')}`
          : '—';
    if (slotDetailStatus) slotDetailStatus.textContent = slot?.active ? 'Active' : 'Muted';
    if (slotDetailType) slotDetailType.textContent = slot?.type ?? '—';
    if (slotDetailChannel) slotDetailChannel.textContent = slot?.midiChannel ?? '—';
    if (slotDetailData) {
      if (slot?.type === 'SysEx') {
        slotDetailData.textContent =
          slot?.sysexTemplate && slot.sysexTemplate.length ? slot.sysexTemplate : '—';
      } else {
        slotDetailData.textContent = slot?.data1 ?? '—';
      }
    }
    const ef = normalizeEf(slot);
    if (slotDetailEfIndex) slotDetailEfIndex.textContent = formatEfIndex(ef.index);
    if (slotDetailEfFilter) slotDetailEfFilter.textContent = formatEfFilter(ef);
    if (slotDetailEfTuning) slotDetailEfTuning.textContent = formatEfTuning(ef);
    if (slotDetailEfDynamics) slotDetailEfDynamics.textContent = formatEfDynamics(ef);
    if (slotDetailEfBaseline) slotDetailEfBaseline.textContent = formatEfBaseline(ef);
    const arg = normalizeArg(slot);
    if (slotDetailArg) slotDetailArg.textContent = formatArgMode(arg);
    if (slotDetailArgSources) slotDetailArgSources.textContent = formatArgSources(arg);
    const value = Array.isArray(telemetry.slots) ? telemetry.slots[slotState.selected] : null;
    if (slotDetailValue) slotDetailValue.textContent = value ?? '—';
    renderSlotEditor();
  }

  // Rebuild the right-hand slot editor for the current selection and UI tier.
  function renderSlotEditor() {
    if (!formContainer) return;
    formContainer.innerHTML = '';
    const slot = slotState.slots[slotState.selected];
    if (!slot) {
      formContainer.textContent = 'Select a slot to edit.';
      return;
    }
    const activeUiMode = getUiMode();
    const activeEditorTab = getEditorTab();
    const form = document.createElement('form');
    form.className = 'slot-editor';
    form.addEventListener('submit', (event) => event.preventDefault());

    const basics = makeFieldset(
      'Knob -> MIDI Mapping',
      'Pick what this knob sends. Switch to Advanced mode for EF/ARG modulation and deep filter controls.'
    );
    basics.appendChild(
      makeSelect(
        'Knob -> MIDI message',
        slotTypeNames,
        slot.type,
        (value) => stageSlotField(slotState.selected, 'type', value),
        { help: glossary.mapping }
      )
    );
    basics.appendChild(
      makeNumber(
        'MIDI channel',
        slot.midiChannel ?? 1,
        1,
        16,
        1,
        (value) => stageSlotField(slotState.selected, 'midiChannel', value),
        { help: 'Use channels 1 to 16 to match your synth or DAW track.' }
      )
    );
    basics.appendChild(
      makeNumber(
        'CC/Note number',
        slot.data1 ?? 0,
        0,
        127,
        1,
        (value) => stageSlotField(slotState.selected, 'data1', value),
        { help: 'The controller number or note number this knob targets.' }
      )
    );
    basics.appendChild(
      makeText(
        'Slot label (browser only)',
        slot.label ?? '',
        'Verse / build / drop cues',
        (value) => runtime.setLocalSlotMeta(slotState.selected, { label: value }),
        { help: glossary.browserLocal }
      )
    );
    if (activeUiMode === 'advanced') {
      basics.appendChild(
        makeNumber('Arp root note', slot.arpNote ?? 0, 0, 127, 1, (value) =>
          stageSlotField(slotState.selected, 'arpNote', value)
        )
      );
    }
    basics.appendChild(
      makeToggle('Enabled', !!slot.active, (value) =>
        stageSlotField(slotState.selected, 'active', value)
      )
    );
    basics.appendChild(
      makeToggle(
        'Knob sends MIDI badge (browser only)',
        !!slot.pot,
        (value) => runtime.setLocalSlotMeta(slotState.selected, { pot: value }),
        { help: glossary.browserLocal }
      )
    );
    const takeover = makeToggle(
      'Take Control (browser only)',
      !!slot.takeover,
      (value) => {
        runtime.setLocalSlotMeta(slotState.selected, { takeover: value });
        runtime.setPotGuard([slotState.selected], !value);
      },
      { help: `${glossary.takeover} ${glossary.browserLocal}` }
    );
    basics.appendChild(takeover);
    if (slot.type === 'SysEx') {
      basics.appendChild(
        makeText(
          'SysEx Template',
          slot.sysexTemplate ?? '',
          'F0 7F 01 04 XX F7',
          (value) => {
            const normalised = normaliseSysexTemplate(value);
            stageSlotField(slotState.selected, 'sysexTemplate', normalised);
          },
          { help: glossary.sysex }
        )
      );
      const hint = document.createElement('p');
      hint.className = 'slot-hint';
      hint.textContent =
        'Hex bytes + XX/MSB/LSB placeholders. We swap the placeholders with live values.';
      basics.appendChild(hint);
    }
    if (activeUiMode !== 'advanced') {
      const hint = document.createElement('p');
      hint.className = 'slot-hint';
      hint.textContent = 'Need EF or ARG modulation? Switch to Advanced mode.';
      basics.appendChild(hint);
    }
    if (activeUiMode !== 'advanced' || activeEditorTab === 'mapping') {
      form.appendChild(basics);
    }

    if (activeUiMode === 'advanced') {
      const manifest = runtime.getState().manifest ?? localManifest;
      const ef = normalizeEf(slot);
      const efSlots = manifest?.envelope_count ?? localManifest?.envelope_count ?? 0;
      const efFieldset = makeFieldset(
        'Envelope Follower (EF)',
        'EF tracks input level so this slot can react to dynamics.'
      );
      efFieldset.appendChild(
        makeNumber(
          'Follower index',
          ef.index ?? -1,
          -1,
          Math.max(-1, efSlots - 1),
          1,
          (value) => {
            stageSlotField(slotState.selected, 'efIndex', value);
            stageSlotEnvelopeField(slotState.selected, 'index', value);
          },
          { help: glossary.ef }
        )
      );
      const currentFilter =
        ef.filter_name ||
        (Number.isFinite(Number(ef.filter_index))
          ? efFilterNames[Number(ef.filter_index)]
          : 'LINEAR');
      efFieldset.appendChild(
        makeSelect(
          'Response shape',
          efFilterNames,
          currentFilter,
          (value) => {
            const idx = Math.max(0, efFilterNames.indexOf(value));
            stageSlotEnvelopeField(slotState.selected, 'filter_name', value);
            stageSlotEnvelopeField(slotState.selected, 'filter_index', idx);
          },
          { help: glossary.filter }
        )
      );
      efFieldset.appendChild(
        makeNumber('Tracking frequency (Hz)', ef.frequency ?? 1000, 0, 20000, 1, (value) =>
          stageSlotEnvelopeField(slotState.selected, 'frequency', value)
        )
      );
      efFieldset.appendChild(
        makeNumber('Resonance (Q)', ef.q ?? 0.707, 0, 10, 0.01, (value) =>
          stageSlotEnvelopeField(slotState.selected, 'q', value)
        )
      );
      efFieldset.appendChild(
        makeNumber('Oversample amount', ef.oversample ?? 4, 1, 32, 1, (value) =>
          stageSlotEnvelopeField(slotState.selected, 'oversample', value)
        )
      );
      efFieldset.appendChild(
        makeNumber('Smoothing', ef.smoothing ?? 0.2, 0, 1, 0.01, (value) =>
          stageSlotEnvelopeField(slotState.selected, 'smoothing', value)
        )
      );
      efFieldset.appendChild(
        makeNumber('Baseline offset', ef.baseline ?? 0, -10, 10, 0.1, (value) =>
          stageSlotEnvelopeField(slotState.selected, 'baseline', value)
        )
      );
      efFieldset.appendChild(
        makeNumber('Gain', ef.gain ?? 1, 0, 8, 0.1, (value) =>
          stageSlotEnvelopeField(slotState.selected, 'gain', value)
        )
      );
      if (activeEditorTab === 'envelope') {
        form.appendChild(efFieldset);
      }

      const arg = normalizeArg(slot);
      const argFieldset = makeFieldset(
        'Follower Combiner (ARG)',
        'ARG blends two followers before this slot sends MIDI.'
      );
      argFieldset.appendChild(
        makeToggle(
          'Enable combiner',
          !!arg.enabled,
          (value) => stageSlotArgField(slotState.selected, 'enabled', value),
          { help: glossary.arg }
        )
      );
      argFieldset.appendChild(
        makeSelect(
          'Combine method',
          argMethodNames,
          resolveArgMethodName(arg) ?? argMethodNames[0],
          (value) => {
            const idx = Math.max(0, argMethodNames.indexOf(value));
            stageSlotArgField(slotState.selected, 'method', idx);
            stageSlotArgField(slotState.selected, 'method_name', value);
          },
          {
            formatOptionLabel: formatArgMethodLabel,
            describeOption: describeArgMethod
          }
        )
      );
      argFieldset.appendChild(
        makeNumber('Follower A', arg.sourceA ?? 0, 0, Math.max(0, efSlots - 1), 1, (value) =>
          stageSlotArgField(slotState.selected, 'sourceA', value)
        )
      );
      argFieldset.appendChild(
        makeNumber('Follower B', arg.sourceB ?? 1, 0, Math.max(0, efSlots - 1), 1, (value) =>
          stageSlotArgField(slotState.selected, 'sourceB', value)
        )
      );
      if (activeEditorTab === 'arg') {
        form.appendChild(argFieldset);
      }
    }

    if (!form.childElementCount) {
      const hint = document.createElement('p');
      hint.className = 'slot-hint';
      hint.textContent = 'Switch tabs to reveal the selected slot section.';
      form.appendChild(hint);
    }

    formContainer.appendChild(form);
  }

  // Build a labeled `<select>` control with optional inline help.
  function makeSelect(
    labelText,
    options,
    current,
    onChange,
    { help, formatOptionLabel, describeOption } = {}
  ) {
    const wrap = document.createElement('label');
    wrap.appendChild(makeControlLabel(labelText, help));
    const select = document.createElement('select');
    options.forEach((opt) => {
      const option = document.createElement('option');
      option.value = opt;
      option.textContent = formatOptionLabel ? formatOptionLabel(opt) : opt;
      if (describeOption) option.title = describeOption(opt);
      if (opt === current) option.selected = true;
      select.appendChild(option);
    });
    select.onchange = () => onChange(select.value);
    wrap.appendChild(select);
    return wrap;
  }

  // Build a numeric input with keyboard-friendly coarse/fine stepping.
  function makeNumber(labelText, current, min, max, step, onCommit, { help } = {}) {
    const wrap = document.createElement('label');
    wrap.appendChild(makeControlLabel(labelText, help));
    const input = document.createElement('input');
    input.type = 'number';
    if (min !== undefined && min !== null) input.min = String(min);
    if (max !== undefined && max !== null) input.max = String(max);
    if (step !== undefined && step !== null) input.step = String(step);
    input.value = current;
    input.onchange = () => onCommit(Number(input.value));
    const coarseStep = step ?? 1;
    attachCoarseFine(input, coarseStep);
    wrap.appendChild(input);
    return wrap;
  }

  // Build a text input wrapper used for labels and SysEx templates.
  function makeText(labelText, current, placeholder, onCommit, { help } = {}) {
    const wrap = document.createElement('label');
    wrap.appendChild(makeControlLabel(labelText, help));
    const input = document.createElement('input');
    input.type = 'text';
    input.value = current ?? '';
    if (placeholder) input.placeholder = placeholder;
    input.addEventListener('change', () => onCommit(input.value));
    wrap.appendChild(input);
    return wrap;
  }

  // Build a checkbox-based toggle control.
  function makeToggle(labelText, current, onCommit, { help } = {}) {
    const wrap = document.createElement('label');
    wrap.className = 'toggle';
    const input = document.createElement('input');
    input.type = 'checkbox';
    input.checked = current;
    input.onchange = () => onCommit(input.checked);
    wrap.append(input, makeControlLabel(labelText, help));
    return wrap;
  }

  // Standardize the visible label line used by all form controls.
  function makeControlLabel(text, helpText) {
    const line = document.createElement('span');
    line.className = 'control-label';
    line.textContent = text;
    appendHelpBadge(line, helpText);
    return line;
  }

  // Attach the little glossary-driven help badge when a control has explainer copy.
  function appendHelpBadge(container, helpText) {
    if (!helpText) return;
    const badge = document.createElement('span');
    badge.className = 'help-badge';
    badge.textContent = '?';
    badge.dataset.tooltip = helpText;
    badge.setAttribute('aria-label', helpText);
    badge.setAttribute('role', 'note');
    container.appendChild(badge);
  }

  // Create one visually consistent fieldset block for the slot editor.
  function makeFieldset(title, hint) {
    const fieldset = document.createElement('fieldset');
    fieldset.className = 'slot-fieldset';
    const legend = document.createElement('legend');
    legend.textContent = title;
    fieldset.appendChild(legend);
    if (hint) {
      const blurb = document.createElement('p');
      blurb.className = 'slot-hint';
      blurb.textContent = hint;
      fieldset.appendChild(blurb);
    }
    return fieldset;
  }

  // Let number fields use Shift+Arrow for coarse adjustments without extra UI chrome.
  function attachCoarseFine(input, baseStep) {
    input.addEventListener('keydown', (event) => {
      if (event.key !== 'ArrowUp' && event.key !== 'ArrowDown') return;
      event.preventDefault();
      const delta =
        (event.key === 'ArrowUp' ? 1 : -1) * (event.shiftKey ? baseStep * 10 : baseStep);
      const next = Math.min(
        Number(input.max),
        Math.max(Number(input.min), Number(input.value) + delta)
      );
      input.value = String(next);
      input.dispatchEvent(new Event('change'));
    });
  }

  // Stage a top-level slot field into the runtime's draft config.
  function stageSlotField(index, key, value) {
    runtime.stage((draft) => {
      draft.slots = draft.slots || [];
      if (!draft.slots[index]) draft.slots[index] = {};
      draft.slots[index][key] = value;
      if (key === 'efIndex') {
        draft.slots[index].ef = draft.slots[index].ef || {};
        draft.slots[index].ef.index = value;
      }
      return draft;
    });
  }

  // Stage an envelope-follower subfield into the selected slot draft.
  function stageSlotEnvelopeField(index, key, value) {
    runtime.stage((draft) => {
      draft.slots = draft.slots || [];
      if (!draft.slots[index]) draft.slots[index] = {};
      draft.slots[index].ef = draft.slots[index].ef || {};
      draft.slots[index].ef[key] = value;
      if (key === 'index') {
        draft.slots[index].efIndex = value;
      }
      return draft;
    });
  }

  // Stage an ARG combiner subfield into the selected slot draft.
  function stageSlotArgField(index, key, value) {
    runtime.stage((draft) => {
      draft.slots = draft.slots || [];
      if (!draft.slots[index]) draft.slots[index] = {};
      draft.slots[index].arg = draft.slots[index].arg || {};
      draft.slots[index].arg[key] = value;
      return draft;
    });
  }

  // Format numbers for compact read-only detail labels.
  function formatNumberField(value, fractionDigits = 2) {
    if (value === null || value === undefined) return '—';
    const num = Number(value);
    if (!Number.isFinite(num)) return '—';
    if (fractionDigits <= 0) return String(Math.round(num));
    return Number(num.toFixed(fractionDigits)).toString();
  }

  // Render an EF index as the user-facing label shown in the detail card.
  function formatEfIndex(index) {
    if (index === null || index === undefined) return '—';
    const num = Number(index);
    if (!Number.isFinite(num)) return '—';
    if (num < 0) return 'Unassigned';
    return `EF ${String(num + 1).padStart(2, '0')}`;
  }

  // Combine the EF filter name and numeric id into one readable label.
  function formatEfFilter(ef) {
    if (!ef) return '—';
    const index = Number(ef.filter_index);
    const name = ef.filter_name || (Number.isFinite(index) ? efFilterNames[index] : null);
    if (!name) return '—';
    const idx = Number.isFinite(index) ? `#${index}` : null;
    return idx ? `${name} (${idx})` : name;
  }

  // Summarize EF frequency/Q tuning for the detail card.
  function formatEfTuning(ef) {
    if (!ef) return '—';
    const freq = formatNumberField(ef.frequency, 1);
    const q = formatNumberField(ef.q, 2);
    if (freq === '—' && q === '—') return '—';
    const freqLabel = freq === '—' ? '—' : `${freq} Hz`;
    return `${freqLabel} • Q ${q}`;
  }

  // Summarize EF dynamics-related tuning for the detail card.
  function formatEfDynamics(ef) {
    if (!ef) return '—';
    const oversample = Number.isFinite(Number(ef.oversample)) ? Number(ef.oversample) : null;
    const smoothing = formatNumberField(ef.smoothing, 2);
    const oversampleLabel = oversample !== null ? `×${oversample}` : '—';
    if (oversampleLabel === '—' && smoothing === '—') return '—';
    return `Oversample ${oversampleLabel} • Smoothing ${smoothing}`;
  }

  // Summarize EF baseline/gain tuning for the detail card.
  function formatEfBaseline(ef) {
    if (!ef) return '—';
    const baseline = formatNumberField(ef.baseline, 2);
    const gain = formatNumberField(ef.gain, 2);
    if (baseline === '—' && gain === '—') return '—';
    return `Baseline ${baseline} • Gain ${gain}`;
  }

  // Resolve the ARG method name from either the saved string or numeric index.
  function resolveArgMethodName(arg) {
    if (!arg || typeof arg !== 'object') return null;
    if (arg.method_name && typeof arg.method_name === 'string') return arg.method_name;
    const index = Number(arg.method);
    if (Number.isFinite(index)) {
      return argMethodNames[index] || null;
    }
    return null;
  }

  // Render the ARG enabled state plus method in one compact label.
  function formatArgMode(arg) {
    if (!arg || typeof arg !== 'object') return '—';
    const methodName = resolveArgMethodName(arg);
    const methodNum = Number(arg.method);
    const methodIndex = Number.isFinite(methodNum) ? `#${methodNum}` : null;
    const enabled = arg.enabled === undefined ? false : Boolean(arg.enabled);
    const pieces = [];
    if (methodName) pieces.push(methodName);
    if (methodIndex && (!methodName || !methodName.includes(methodIndex))) pieces.push(methodIndex);
    pieces.push(enabled ? 'ON' : 'OFF');
    return pieces.join(' · ');
  }

  // Convert an EF index into the route label shown by ARG summaries.
  function formatEfRoute(index) {
    if (index === null || index === undefined) return '—';
    const num = Number(index);
    if (!Number.isFinite(num) || num < 0) return '—';
    return `EF ${String(num + 1).padStart(2, '0')}`;
  }

  // Summarize the two ARG source followers in the detail card.
  function formatArgSources(arg) {
    if (!arg || typeof arg !== 'object') return '—';
    const sourceA = formatEfRoute(arg.sourceA);
    const sourceB = formatEfRoute(arg.sourceB);
    if (sourceA === '—' && sourceB === '—') return '—';
    return `A → ${sourceA} • B → ${sourceB}`;
  }

  // Fill in missing EF defaults and normalize mixed legacy/current field shapes.
  function normalizeEf(slot) {
    const base = slot?.ef ? { ...slot.ef } : {};
    const defaults = {
      frequency: 1000,
      q: 0.707,
      oversample: 4,
      smoothing: 0.2,
      baseline: 0,
      gain: 1
    };
    const index = Number.isFinite(slot?.efIndex)
      ? Number(slot.efIndex)
      : Number.isFinite(base.index)
        ? Number(base.index)
        : -1;
    if (!Number.isFinite(base.index)) base.index = index;
    if (Number.isFinite(Number(base.filter_index))) {
      base.filter_index = Number(base.filter_index);
    }
    if (!Number.isFinite(base.filter_index) && typeof base.filter_name === 'string') {
      const idx = efFilterNames.indexOf(base.filter_name);
      if (idx >= 0) base.filter_index = idx;
    }
    if (!base.filter_name && Number.isFinite(base.filter_index)) {
      base.filter_name = efFilterNames[base.filter_index] || null;
    }
    base.frequency = Number.isFinite(Number(base.frequency))
      ? Number(base.frequency)
      : defaults.frequency;
    base.q = Number.isFinite(Number(base.q)) ? Number(base.q) : defaults.q;
    base.oversample = Number.isFinite(Number(base.oversample))
      ? Math.max(1, Math.round(Number(base.oversample)))
      : defaults.oversample;
    const smoothing = Number(base.smoothing);
    base.smoothing = Number.isFinite(smoothing)
      ? Math.max(0, Math.min(1, smoothing))
      : defaults.smoothing;
    base.baseline = Number.isFinite(Number(base.baseline))
      ? Number(base.baseline)
      : defaults.baseline;
    base.gain = Number.isFinite(Number(base.gain)) ? Number(base.gain) : defaults.gain;
    base.index = index;
    return base;
  }

  // Fill in missing ARG defaults and clamp sources against the current manifest.
  function normalizeArg(slot) {
    const base = slot?.arg ? { ...slot.arg } : {};
    if (base.enabled === undefined) base.enabled = false;
    const methodIndex = (() => {
      if (base.method_name && typeof base.method_name === 'string') {
        const idx = argMethodNames.indexOf(base.method_name);
        if (idx >= 0) return idx;
      }
      const idx = Number(base.method);
      return Number.isFinite(idx) ? idx : 0;
    })();
    base.method = methodIndex;
    base.method_name = argMethodNames[methodIndex] || base.method_name || 'PLUS';
    const manifest = runtime.getState().manifest ?? localManifest;
    const efLimit = Math.max(
      0,
      (manifest?.envelope_count ?? localManifest?.envelope_count ?? 6) - 1
    );
    const sanitizeSource = (value, fallback) => {
      const num = Number(value);
      if (!Number.isFinite(num)) return fallback;
      return Math.max(0, Math.min(efLimit, num));
    };
    base.sourceA = sanitizeSource(base.sourceA, 0);
    base.sourceB = sanitizeSource(base.sourceB, Math.min(1, efLimit));
    return base;
  }

  // Normalize a typed SysEx template into the token format runtime transport expects.
  function normaliseSysexTemplate(value) {
    if (typeof value !== 'string') return '';
    const trimmed = value.trim();
    if (!trimmed.length) return '';
    return trimmed
      .split(/\s+/)
      .map((token) => {
        if (!token.length) return null;
        if (/^(xx|msb|lsb)$/i.test(token)) return token.toUpperCase();
        return token.toUpperCase();
      })
      .filter(Boolean)
      .join(' ');
  }

  return {
    populateDetail,
    renderSlotEditor
  };
}
