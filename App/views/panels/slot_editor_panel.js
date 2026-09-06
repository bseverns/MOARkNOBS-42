import { renderSlotSignal } from '../slot_signal_path.js';
import { updateStagedControlMarkers, normalizeConfigPath } from '../staged_control_markers.js';
import {
  EF_DESTINATION_PRESENTATION,
  SLOT_TUNING_RECIPES,
  applySlotTuningRecipe,
  describeEfFilter,
  formatEfDestinationLabel,
  formatEfFilterLabel
} from '../../lib/tuning_catalog.js';

const SLOT_DATA1_PRESENTATION = Object.freeze({
  CC: {
    label: 'CC number',
    help: 'The Control Change number this knob sends.'
  },
  Note: {
    label: 'Note number',
    help: 'The MIDI note identity used when this slot sends notes.'
  },
  ProgramChange: {
    label: 'Program number',
    help: 'The program number recalled when this knob triggers Program Change.'
  },
  NRPN: {
    label: 'NRPN parameter',
    help: 'The stored 7-bit parameter component used by the firmware for NRPN selection.'
  },
  RPN: {
    label: 'RPN parameter',
    help: 'The stored 7-bit parameter component used by the firmware for RPN selection.'
  }
});

const LFO_GENERATOR_SHAPES = ['Sine', 'Triangle', 'Saw', 'Square', 'Sample & Hold', 'Random Slew'];

function formatSharedGenerator(entry, index) {
  const fallback = index === 0 ? { shape: 0, frequency_hz: 1 } : { shape: 1, frequency_hz: 0.5 };
  const shape = LFO_GENERATOR_SHAPES[Number(entry?.shape)] ?? LFO_GENERATOR_SHAPES[fallback.shape];
  const rate = Number(entry?.frequency_hz ?? fallback.frequency_hz);
  const rateLabel = Number.isFinite(rate) ? rate.toFixed(rate < 10 ? 2 : 1) : String(fallback.frequency_hz);
  return `${shape} · ${rateLabel} Hz · shared generator`;
}

export function createSlotEditorPanel({
  runtime,
  localManifest,
  slotState,
  formContainer,
  noteDynamicsCard = null,
  noteDynamicsParking = null,
  detailElements = {},
  glossary = {},
  slotTypeNames = [],
  efFilterNames = [],
  argMethodNames = [],
  formatArgMethodLabel,
  describeArgMethod,
  setStatus = () => {},
  getUiMode = () => 'basic',
  getEditorTab = () => 'mapping',
  getSharedLfos = () => [],
  openLabTab = () => {},
  openLfoGenerator = () => {}
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
    slotDetailLfo = null,
    slotDetailValue = null
  } = detailElements;
  const efModeNames = ['PEAK', 'RMS', 'GATE', 'FOLLOWER'];
  const efModeDescriptions = {
    PEAK: 'Peak detector with attack and release timing.',
    RMS: 'Energy detector using a leaky RMS-style integration window.',
    GATE: 'Threshold gate with hysteresis for on/off style dynamics.',
    FOLLOWER: 'Attack/release follower for smooth contour tracking.'
  };
  const efDestinationOptions = [
    { value: 'add_clamp', label: 'Add' },
    { value: 'subtract', label: 'Subtract' },
    { value: 'replace', label: 'Replace' },
    { value: 'scale', label: 'Scale' },
    { value: 'centered', label: 'Centered' }
  ];
  const efDestinationValues = efDestinationOptions.map((option) => option.value);
  const lfoModeOptions = [
    { value: 0, label: 'Add', description: 'Add the unipolar 0…1 LFO value.' },
    { value: 1, label: 'Subtract', description: 'Subtract the unipolar 0…1 LFO value.' },
    { value: 2, label: 'Replace', description: 'Replace the slot value around MIDI center 64.' },
    { value: 3, label: 'Scale', description: 'Scale the preceding value with bipolar motion.' },
    { value: 4, label: 'Centered', description: 'Move around the knob baseline by −64…+63.' }
  ];
  const lfoModeValues = lfoModeOptions.map((option) => option.value);
  let slotEditorRenderPending = false;
  let slotEditorFocusGuardBound = false;
  let lastRecipeResult = '';
  const recipeSelections = new Map();
  const efLastActiveAt = new Map();
  const motionDetailsOpen = new Map();

  function bindSlotEditorFocusGuard() {
    if (!formContainer || slotEditorFocusGuardBound) return;
    slotEditorFocusGuardBound = true;
    formContainer.addEventListener('focusout', () => {
      setTimeout(() => {
        if (!slotEditorRenderPending || isSlotEditorEditing()) return;
        slotEditorRenderPending = false;
        renderSlotEditor();
      }, 0);
    });
  }

  function isSlotEditorEditing() {
    const active = document.activeElement;
    if (!formContainer || !active || !formContainer.contains(active)) return false;
    return ['INPUT', 'SELECT', 'TEXTAREA', 'BUTTON'].includes(active.tagName);
  }

  // Fill the slot detail card from the selected slot plus latest telemetry.
  function populateDetail({ renderEditor = true } = {}) {
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
    if (slotDetailLfo) slotDetailLfo.textContent = formatSlotLfoSummary(slot);
    const value = Array.isArray(telemetry.slots) ? telemetry.slots[slotState.selected] : null;
    if (slotDetailValue) slotDetailValue.textContent = value ?? '—';
    updateSignalPath();
    if (renderEditor) renderSlotEditor();
    else updateSelectedTuningEvidence();
  }

  // Rebuild the right-hand slot editor for the current selection and UI tier.
  function renderSlotEditor() {
    if (!formContainer) return;
    bindSlotEditorFocusGuard();
    updateStagedControlMarkers(formContainer, runtime.diff());
    if (isSlotEditorEditing()) {
      slotEditorRenderPending = true;
      return;
    }
    slotEditorRenderPending = false;
    parkNoteDynamicsCard();
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
      'Choose what leaves the instrument. Reactive, Combine, and Motion shape the value before it is sent.'
    );
    basics.appendChild(
      makeSelect(
        'Knob -> MIDI message',
        slotTypeNames,
        slot.type,
        (value) => stageSlotField(slotState.selected, 'type', value),
        { help: glossary.mapping, configPaths: 'slots.*.type' }
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
        {
          help: 'Use channels 1 to 16 to match your synth or DAW track.',
          configPaths: 'slots.*.midiChannel'
        }
      )
    );
    const data1Presentation = SLOT_DATA1_PRESENTATION[slot.type];
    if (data1Presentation) {
      basics.appendChild(
        makeNumber(
          data1Presentation.label,
          slot.data1 ?? 0,
          0,
          127,
          1,
          (value) => stageSlotField(slotState.selected, 'data1', value),
          {
            help: data1Presentation.help,
            configPaths: 'slots.*.data1'
          }
        )
      );
    }
    basics.appendChild(
      makeText(
        'Slot label',
        slot.label ?? '',
        'Verse / build / drop cues',
        (value) => runtime.setLocalSlotMeta(slotState.selected, { label: value }),
        { help: glossary.browserLocal }
      )
    );
    if (activeUiMode === 'advanced') {
      basics.appendChild(
        makeNumber(
          'Arp root note',
          slot.arpNote ?? 0,
          0,
          127,
          1,
          (value) => stageSlotField(slotState.selected, 'arpNote', value),
          { configPaths: 'slots.*.arpNote' }
        )
      );
      if (slot.type === 'Note') {
        const arpHint = document.createElement('p');
        arpHint.className = 'slot-hint';
        arpHint.textContent =
          'Arpeggiator start/stop and live timing controls live in the Arp panel.';
        basics.appendChild(arpHint);
        attachNoteDynamicsCard(basics);
      }
    }
    basics.appendChild(
      makeToggle(
        'Enabled',
        !!slot.active,
        (value) => stageSlotField(slotState.selected, 'active', value),
        { configPaths: 'slots.*.active' }
      )
    );
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
          { help: glossary.sysex, configPaths: 'slots.*.sysexTemplate' }
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
      hint.textContent =
        'Configure translates common musical choices; Lab keeps every exact parameter.';
      basics.appendChild(hint);
    }
    if (activeUiMode !== 'advanced' || activeEditorTab === 'mapping') {
      form.appendChild(basics);
    }

    if (activeUiMode === 'basic') {
      const modulation = document.createElement('div');
      modulation.className = 'configure-modulation';
      modulation.append(
        makeConfigureTuningSurface(slot),
        makeConfigureMotionSurface(slot)
      );
      form.appendChild(modulation);
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
          {
            help: glossary.ef,
            configPaths: ['slots.*.efIndex', 'slots.*.ef.index']
          }
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
          {
            help: glossary.filter,
            formatOptionLabel: formatEfFilterLabel,
            describeOption: describeEfFilter,
            configPaths: ['slots.*.ef.filter_index', 'slots.*.ef.filter_name']
          }
        )
      );
      efFieldset.appendChild(
        makeNumber(
          'Tracking frequency control (legacy scale)',
          ef.frequency ?? 1000,
          20,
          5000,
          1,
          (value) => stageSlotEnvelopeField(slotState.selected, 'frequency', value),
          { configPaths: 'slots.*.ef.frequency' }
        )
      );
      efFieldset.appendChild(
        makeNumber(
          'Resonance (Q)',
          ef.q ?? 0.707,
          0.5,
          4,
          0.01,
          (value) => stageSlotEnvelopeField(slotState.selected, 'q', value),
          { configPaths: 'slots.*.ef.q' }
        )
      );
      efFieldset.appendChild(
        makeNumber(
          'Oversample amount',
          ef.oversample ?? 4,
          1,
          32,
          1,
          (value) => stageSlotEnvelopeField(slotState.selected, 'oversample', value),
          { configPaths: 'slots.*.ef.oversample' }
        )
      );
      efFieldset.appendChild(makeDeckShortcut('Ctrl3 double', 'Cycle EF oversampling'));
      efFieldset.appendChild(
        makeNumber(
          'Smoothing',
          ef.smoothing ?? 0.2,
          0,
          1,
          0.01,
          (value) => stageSlotEnvelopeField(slotState.selected, 'smoothing', value),
          { configPaths: 'slots.*.ef.smoothing' }
        )
      );
      const efModeName = resolveEfModeName(ef) ?? efModeNames[0];
      efFieldset.appendChild(
        makeSelect(
          'Detection mode',
          efModeNames,
          efModeName,
          (value) => {
            const idx = Math.max(0, efModeNames.indexOf(value));
            stageSlotEnvelopeField(slotState.selected, 'mode', idx);
          },
          {
            help: 'Choose how the follower detects level before modulation.',
            formatOptionLabel: formatEfModeLabel,
            describeOption: (value) => efModeDescriptions[value] ?? value,
            configPaths: 'slots.*.ef.mode'
          }
        )
      );
      if (efModeName === 'PEAK' || efModeName === 'FOLLOWER') {
        efFieldset.appendChild(
          makeNumber(
            'Attack time (ms)',
            ef.attackMs ?? 5,
            1,
            60000,
            1,
            (value) => stageSlotEnvelopeField(slotState.selected, 'attackMs', value),
            { configPaths: 'slots.*.ef.attackMs' }
          )
        );
        efFieldset.appendChild(
          makeNumber(
            'Release time (ms)',
            ef.releaseMs ?? 20,
            1,
            60000,
            1,
            (value) => stageSlotEnvelopeField(slotState.selected, 'releaseMs', value),
            { configPaths: 'slots.*.ef.releaseMs' }
          )
        );
      }
      if (efModeName === 'RMS') {
        efFieldset.appendChild(
          makeNumber(
            'RMS window (ms)',
            ef.rmsWindowMs ?? 50,
            1,
            60000,
            1,
            (value) => stageSlotEnvelopeField(slotState.selected, 'rmsWindowMs', value),
            { configPaths: 'slots.*.ef.rmsWindowMs' }
          )
        );
      }
      if (efModeName === 'GATE') {
        efFieldset.appendChild(
          makeNumber(
            'Gate threshold',
            ef.gateThreshold ?? 16,
            0,
            127,
            1,
            (value) => stageSlotEnvelopeField(slotState.selected, 'gateThreshold', value),
            { configPaths: 'slots.*.ef.gateThreshold' }
          )
        );
        efFieldset.appendChild(
          makeNumber(
            'Gate hysteresis',
            ef.gateHysteresis ?? 4,
            0,
            127,
            1,
            (value) => stageSlotEnvelopeField(slotState.selected, 'gateHysteresis', value),
            { configPaths: 'slots.*.ef.gateHysteresis' }
          )
        );
      }
      efFieldset.appendChild(
        makeToggle(
          'Auto-baseline',
          !!ef.autoBaseline,
          (value) => stageSlotEnvelopeField(slotState.selected, 'autoBaseline', value),
          { configPaths: 'slots.*.ef.autoBaseline' }
        )
      );
      efFieldset.appendChild(
        makeToggle(
          'Auto-gain',
          !!ef.autoGain,
          (value) => stageSlotEnvelopeField(slotState.selected, 'autoGain', value),
          { configPaths: 'slots.*.ef.autoGain' }
        )
      );
      efFieldset.appendChild(
        makeNumber(
          'Activity threshold',
          ef.activityThreshold ?? 4,
          0,
          127,
          1,
          (value) => stageSlotEnvelopeField(slotState.selected, 'activityThreshold', value),
          { configPaths: 'slots.*.ef.activityThreshold' }
        )
      );
      efFieldset.appendChild(
        makeNumber(
          'Baseline time constant (ms)',
          ef.baselineTauMs ?? 2000,
          1,
          60000,
          1,
          (value) => stageSlotEnvelopeField(slotState.selected, 'baselineTauMs', value),
          { configPaths: 'slots.*.ef.baselineTauMs' }
        )
      );
      efFieldset.appendChild(
        makeNumber(
          'Gain time constant (ms)',
          ef.gainTauMs ?? 3000,
          1,
          60000,
          1,
          (value) => stageSlotEnvelopeField(slotState.selected, 'gainTauMs', value),
          { configPaths: 'slots.*.ef.gainTauMs' }
        )
      );
      efFieldset.appendChild(
        makeNumber(
          'Auto-gain target',
          ef.gainTarget ?? 102,
          0,
          127,
          1,
          (value) => stageSlotEnvelopeField(slotState.selected, 'gainTarget', value),
          { configPaths: 'slots.*.ef.gainTarget' }
        )
      );
      efFieldset.appendChild(
        makeNumber(
          'Baseline offset',
          ef.baseline ?? 0,
          -10,
          10,
          0.1,
          (value) => stageSlotEnvelopeField(slotState.selected, 'baseline', value),
          { configPaths: 'slots.*.ef.baseline' }
        )
      );
      efFieldset.appendChild(
        makeNumber(
          'Gain',
          ef.gain ?? 1,
          0,
          8,
          0.1,
          (value) => stageSlotEnvelopeField(slotState.selected, 'gain', value),
          { configPaths: 'slots.*.ef.gain' }
        )
      );
      efFieldset.appendChild(
        makeSelect(
          'Destination mode',
          efDestinationValues,
          resolveEfDestinationMode(ef),
          (value) => stageSlotEnvelopeField(slotState.selected, 'destination_mode', value),
          {
            help: 'Choose how EF modulation combines with the base MIDI value.',
            formatOptionLabel: (value) =>
              efDestinationOptions.find((option) => option.value === value)?.label ?? value,
            configPaths: 'slots.*.ef.destination_mode'
          }
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
          { help: glossary.arg, configPaths: 'slots.*.arg.enabled' }
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
            describeOption: describeArgMethod,
            configPaths: ['slots.*.arg.method', 'slots.*.arg.method_name']
          }
        )
      );
      argFieldset.appendChild(makeRecipeButtons('arg'));
      argFieldset.appendChild(makeDeckShortcut('Ctrl4 double', 'Toggle this slot’s ARG combiner'));
      argFieldset.appendChild(
        makeNumber(
          'Follower A',
          arg.sourceA ?? 0,
          0,
          Math.max(0, efSlots - 1),
          1,
          (value) => stageSlotArgField(slotState.selected, 'sourceA', value),
          { configPaths: 'slots.*.arg.sourceA' }
        )
      );
      argFieldset.appendChild(
        makeNumber(
          'Follower B',
          arg.sourceB ?? 1,
          0,
          Math.max(0, efSlots - 1),
          1,
          (value) => stageSlotArgField(slotState.selected, 'sourceB', value),
          { configPaths: 'slots.*.arg.sourceB' }
        )
      );
      if (activeEditorTab === 'arg') {
        form.appendChild(argFieldset);
      }

      const lfoFieldset = makeFieldset(
        'Fixed LFO Lanes',
        'LFO 1 and LFO 2 compose after EF/ARG. Centered keeps the knob as the midpoint.'
      );
      normalizeSlotLfo(slot).forEach((lane, laneIndex) => {
        const card = document.createElement('section');
        card.className = 'slot-lfo-lane';
        card.dataset.lfoLane = String(laneIndex);
        const heading = document.createElement('h4');
        heading.textContent = `LFO ${laneIndex + 1}`;
        card.appendChild(heading);
        card.appendChild(
          makeToggle(
            `Enable LFO ${laneIndex + 1}`,
            lane.enabled,
            (value) => stageSlotLfoField(slotState.selected, laneIndex, 'enabled', value),
            {
              help: `Fixed lane ${laneIndex + 1} always reads LFO ${laneIndex + 1}.`,
              configPaths: 'slots.*.lfo.*.enabled'
            }
          )
        );
        card.appendChild(
          makeSelect(
            `LFO ${laneIndex + 1} combine mode`,
            lfoModeValues,
            lane.mode,
            (value) => stageSlotLfoField(slotState.selected, laneIndex, 'mode', Number(value)),
            {
              help: 'Choose how this lane composes with the value produced before it.',
              formatOptionLabel: (value) =>
                lfoModeOptions.find((option) => option.value === value)?.label ?? value,
              describeOption: (value) =>
                lfoModeOptions.find((option) => option.value === value)?.description ?? '',
              configPaths: 'slots.*.lfo.*.mode'
            }
          )
        );
        card.appendChild(
          makeNumber(
            `LFO ${laneIndex + 1} amount (%)`,
            lane.amount,
            -100,
            100,
            1,
            (value) => stageSlotLfoField(slotState.selected, laneIndex, 'amount', value),
            {
              help: 'Signed depth. Negative values reverse the selected operation.',
              configPaths: 'slots.*.lfo.*.amount'
            }
          )
        );
        card.appendChild(makeRecipeButtons('lfo', laneIndex));
        if (laneIndex === 0) {
          card.appendChild(makeDeckShortcut('Ctrl5 double', 'Toggle this slot’s fixed LFO 1 lane'));
        }
        card.querySelectorAll('[data-staged-path]').forEach((node) => {
          node.dataset.stagedPath = node.dataset.stagedPath.replaceAll('lfo.*', `lfo.${laneIndex}`);
        });
        lfoFieldset.appendChild(card);
      });
      if (activeEditorTab === 'lfo') {
        form.appendChild(lfoFieldset);
      }
    }

    if (!form.childElementCount) {
      const hint = document.createElement('p');
      hint.className = 'slot-hint';
      hint.textContent = 'Switch tabs to reveal the selected slot section.';
      form.appendChild(hint);
    }

    formContainer.appendChild(form);
    updateStagedControlMarkers(formContainer, runtime.diff());
    updateSignalPath();
  }

  function makeConfigureTuningSurface(slot) {
    const ef = normalizeEf(slot);
    const manifest = runtime.getState().manifest ?? localManifest;
    const followerCount = Math.max(0, Number(manifest?.envelope_count) || 0);
    const fieldset = makeFieldset(
      'Reactive · Tune This Slot',
      'Choose a source and a starting character. These actions stage exact Lab parameters for review before Apply.'
    );
    fieldset.classList.add('configure-tuning');
    fieldset.dataset.configureZone = 'envelope';

    const sourceOptions = [-1, ...Array.from({ length: followerCount }, (_, index) => index)];
    fieldset.appendChild(
      makeSelect(
        'Source',
        sourceOptions,
        Number(ef.index),
        (value) => {
          const next = Number(value);
          stageSlotField(slotState.selected, 'efIndex', next);
          stageSlotEnvelopeField(slotState.selected, 'index', next);
        },
        {
          help: 'Select which envelope follower moves this slot. Unassigned leaves EF modulation disconnected.',
          configPaths: ['slots.*.efIndex', 'slots.*.ef.index'],
          formatOptionLabel: (value) =>
            Number(value) < 0 ? 'Unassigned' : `EF ${Number(value) + 1}`
        }
      )
    );

    fieldset.appendChild(makeConfigureArgSurface(slot));

    const character = document.createElement('section');
    character.className = 'tuning-character';
    character.dataset.stagedPath = `slots.${slotState.selected}.ef`;
    const characterLabel = document.createElement('strong');
    characterLabel.textContent = 'Character';
    const current = document.createElement('p');
    current.className = 'tuning-character-current';
    current.textContent = formatEfFilterLabel(ef.filter_name ?? 'LINEAR');
    character.append(characterLabel, current, makeRecipeButtons('ef'));
    fieldset.appendChild(character);

    const response = document.createElement('p');
    response.className = 'tuning-response-summary';
    response.textContent = formatResponseSummary(ef);
    fieldset.appendChild(response);

    const amount = document.createElement('p');
    amount.className = 'tuning-amount-summary';
    amount.textContent = formatAmountSummary(ef);
    fieldset.appendChild(amount);
    fieldset.appendChild(
      makeSelect(
        'Amount',
        ['adaptive', 'subtle', 'moderate', 'strong'],
        ef.autoGain ? 'adaptive' : ef.gain < 0.8 ? 'subtle' : ef.gain > 1.5 ? 'strong' : 'moderate',
        (value) =>
          runtime.stage((draft) => {
            const target = draft.slots[slotState.selected].ef;
            target.autoGain = value === 'adaptive';
            if (value !== 'adaptive') target.gain = { subtle: 0.5, moderate: 1, strong: 2 }[value];
            return draft;
          }),
        { configPaths: ['slots.*.ef.autoGain', 'slots.*.ef.gain'] }
      )
    );

    fieldset.appendChild(
      makeSelect(
        'Direction',
        Object.keys(EF_DESTINATION_PRESENTATION),
        resolveEfDestinationMode(ef),
        (value) => stageSlotEnvelopeField(slotState.selected, 'destination_mode', value),
        {
          help: 'Direction maps directly to the firmware destination mode shown after the musical label.',
          configPaths: 'slots.*.ef.destination_mode',
          formatOptionLabel: (value) => EF_DESTINATION_PRESENTATION[value]?.musicalLabel || value
        }
      )
    );

    fieldset.appendChild(makeSelectedTuningEvidence(ef));

    const recipeResult = document.createElement('p');
    recipeResult.className = 'tuning-recipe-result';
    recipeResult.setAttribute('aria-live', 'polite');
    recipeResult.textContent = recipeSelections.get(`${slotState.selected}:ef:0`)?.result || '';
    fieldset.appendChild(recipeResult);

    if (runtime.getState().canStagePreviousApply) {
      const returnButton = document.createElement('button');
      returnButton.type = 'button';
      returnButton.textContent = 'Return to pre-Apply state';
      returnButton.title =
        'Stage the last confirmed state from immediately before Apply. Review and Apply again to send it.';
      returnButton.addEventListener('click', () => {
        returnButton.blur();
        if (!runtime.stagePreviousApply()) return;
        setStatus(
          'warn',
          'Previous state staged',
          'Nothing was written yet. Review the diff, then Apply to return the device.'
        );
      });
      fieldset.appendChild(returnButton);
    }

    const customize = document.createElement('button');
    customize.type = 'button';
    customize.className = 'tuning-customize';
    customize.textContent = 'Customize in Lab';
    customize.addEventListener('click', () => {
      customize.blur();
      openLabTab('envelope');
    });
    fieldset.appendChild(customize);
    return fieldset;
  }

  function makeCustomizeButton(tab, laneIndex = null, text = 'Customize in Lab') {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'tuning-customize';
    button.textContent = text;
    button.addEventListener('click', () => {
      button.blur();
      openLabTab(tab, laneIndex);
    });
    return button;
  }

  function makeConfigureArgSurface(slot) {
    const arg = normalizeArg(slot);
    const section = document.createElement('section');
    section.className = 'reactive-relationship';
    section.dataset.configureZone = 'arg';
    const heading = document.createElement('strong');
    heading.textContent = 'Relationship';
    const description = document.createElement('p');
    description.className = 'microcopy';
    description.textContent =
      'ARG relates two envelope followers before this slot’s EF shaping. It constructs the reactive source; it is not a separate modulation stage.';
    section.append(
      heading,
      description,
      makeToggle(
        'Use two followers',
        arg.enabled,
        (value) => stageSlotArgField(slotState.selected, 'enabled', value),
        { configPaths: 'slots.*.arg.enabled' }
      )
    );
    const current = document.createElement('p');
    current.className = 'relationship-summary';
    current.textContent = arg.enabled
      ? formatArgMethodLabel(arg.method_name).split(' · ')[0]
      : 'Off · one assigned follower';
    current.dataset.stagedPath = `slots.${slotState.selected}.arg.method slots.${slotState.selected}.arg.method_name`;
    section.append(current, makeRecipeButtons('arg'));
    const count = runtime.getState().manifest?.envelope_count ?? localManifest.envelope_count ?? 6;
    const followers = Array.from({ length: count }, (_, index) => index);
    ['sourceA', 'sourceB'].forEach((key, index) => {
      section.appendChild(
        makeSelect(
          `Follower ${index ? 'B' : 'A'}`,
          followers,
          arg[key],
          (value) => stageSlotArgField(slotState.selected, key, Number(value)),
          {
            configPaths: `slots.*.arg.${key}`,
            formatOptionLabel: (value) => `EF ${Number(value) + 1}`
          }
        )
      );
    });
    section.appendChild(makeCustomizeButton('arg', null, 'Customize ARG in Lab'));
    return section;
  }

  function makeConfigureMotionSurface(slot) {
    const fieldset = makeFieldset(
      'Motion · LFO',
      'Two fixed lanes, applied in order. Each lane shows its shared generator here; edit that generator in Lab’s Profile LFO & Routes.'
    );
    fieldset.classList.add('configure-motion');
    fieldset.dataset.configureZone = 'lfo';
    const sharedLfos = getSharedLfos();
    normalizeSlotLfo(slot).forEach((lane, index) => {
      const card = document.createElement('section');
      card.className = 'configure-lfo-lane';
      card.dataset.lfoLane = String(index);
      const title = document.createElement('h4');
      title.textContent = `LFO ${index + 1}`;
      const generator = document.createElement('p');
      generator.className = 'shared-generator-summary';
      generator.textContent = formatSharedGenerator(sharedLfos[index], index);
      card.append(
        title,
        generator,
        makeToggle(
          `Use LFO ${index + 1}`,
          lane.enabled,
          (value) => stageSlotLfoField(slotState.selected, index, 'enabled', value),
          { configPaths: `slots.*.lfo.${index}.enabled` }
        ),
        makeNumber(
          `LFO ${index + 1} depth (%)`,
          lane.amount,
          -100,
          100,
          1,
          (value) => stageSlotLfoField(slotState.selected, index, 'amount', value),
          { configPaths: `slots.*.lfo.${index}.amount` }
        )
      );
      const detail = document.createElement('details');
      detail.className = 'motion-composition';
      const detailKey = `${slotState.selected}:${index}`;
      detail.open = Boolean(motionDetailsOpen.get(detailKey));
      detail.addEventListener('toggle', () => {
        if (detail.isConnected) motionDetailsOpen.set(detailKey, detail.open);
      });
      const summary = document.createElement('summary');
      summary.textContent = 'Movement & recipes';
      detail.append(
        summary,
        makeSelect(
          `LFO ${index + 1} movement`,
          lfoModeValues,
          lane.mode,
          (value) => stageSlotLfoField(slotState.selected, index, 'mode', Number(value)),
          {
            configPaths: `slots.*.lfo.${index}.mode`,
            formatOptionLabel: (value) =>
              ['Add movement', 'Pull down', 'Take over', 'Scale with motion', 'Around the knob'][
                value
              ]
          }
        ),
        makeRecipeButtons('lfo', index),
        makeCustomizeButton('lfo', index)
      );
      const editGenerator = document.createElement('button');
      editGenerator.type = 'button';
      editGenerator.className = 'shared-generator-link';
      editGenerator.textContent = 'Edit generator in Lab →';
      editGenerator.addEventListener('click', () => {
        editGenerator.blur();
        openLfoGenerator(index);
      });
      card.append(detail, editGenerator);
      fieldset.appendChild(card);
    });
    return fieldset;
  }

  function updateSignalPath() {
    const index = slotState.selected;
    const slot = slotState.slots[index];
    const container = document.getElementById('selected-slot-signal');
    if (!container || !slot) return;
    renderSlotSignal(container, {
      slot,
      liveSlot: runtime.getState().live?.slots?.[index],
      telemetry: slotState.telemetry,
      index,
      connected: document.documentElement.dataset.connected === 'true',
      dirty: runtime
        .diff()
        .some((change) => normalizeConfigPath(change.path).startsWith(`slots.${index}.`)),
      onNavigate: (destination) => {
        const paths = {
          reactive: `slots.${index}.ef.index`,
          'lfo-0': `slots.${index}.lfo.0.enabled`,
          'lfo-1': `slots.${index}.lfo.1.enabled`
        };
        focusControl(paths[destination], destination.startsWith('lfo-') ? Number(destination.at(-1)) : null);
      }
    });
  }

  function focusControl(path, laneIndex = null) {
    document.activeElement?.blur?.();
    renderSlotEditor();
    const normalized = normalizeConfigPath(path);
    const nodes = [...formContainer.querySelectorAll('[data-staged-path]')];
    const target =
      nodes.find((node) => node.dataset.stagedPath.split(' ').includes(normalized)) ||
      (laneIndex !== null ? formContainer.querySelector(`[data-lfo-lane="${laneIndex}"]`) : null) ||
      formContainer;
    target.classList.add('control-focus-target');
    target.scrollIntoView({ block: 'center' });
    target.querySelector('input:not([type="range"]), select, button')?.focus();
    window.setTimeout(() => target.classList.remove('control-focus-target'), 1800);
  }

  function makeRecipeButtons(target, laneIndex = 0) {
    const container = document.createElement('div');
    container.className = 'tuning-recipes';
    container.setAttribute('role', 'group');
    container.setAttribute('aria-label', `${target.toUpperCase()} tuning recipes`);
    const recipes = SLOT_TUNING_RECIPES.filter((recipe) => recipe.target === target);
    const selection = recipeSelections.get(`${slotState.selected}:${target}:${laneIndex}`);
    const current =
      target === 'lfo'
        ? slotState.slots[slotState.selected]?.lfo?.[laneIndex]
        : slotState.slots[slotState.selected]?.[target];
    const selected =
      recipes.find(
        (recipe) =>
          recipe.id === selection?.id &&
          Object.entries(recipe.patch).every(([key, value]) => current?.[key] === value)
      ) ||
      recipes.find((recipe) =>
        Object.entries(recipe.patch).every(([key, value]) => current?.[key] === value)
      );
    recipes.forEach((recipe) => {
      const button = document.createElement('button');
      button.type = 'button';
      button.dataset.recipeId = recipe.id;
      button.textContent = recipe.label;
      button.title = recipe.explanation;
      button.setAttribute('aria-pressed', String(recipe.id === selected?.id));
      button.addEventListener('click', () => {
        button.blur();
        stageTuningRecipe(recipe.id, laneIndex);
      });
      container.appendChild(button);
    });
    const explanation = document.createElement('p');
    explanation.className = 'recipe-explanation';
    explanation.setAttribute('role', 'status');
    explanation.textContent = selected
      ? `${selected.label} — ${selected.explanation}`
      : 'Custom settings. Choose a recipe to stage a musical starting point.';
    container.appendChild(explanation);
    return container;
  }

  function stageTuningRecipe(recipeId, laneIndex = 0) {
    let stagedResult = null;
    runtime.stage((draft) => {
      draft.slots = draft.slots || [];
      const current = draft.slots[slotState.selected] || {};
      stagedResult = applySlotTuningRecipe(current, recipeId, { laneIndex });
      draft.slots[slotState.selected] = stagedResult.slot;
      lastRecipeResult = formatRecipeResult(stagedResult);
      recipeSelections.set(`${slotState.selected}:${stagedResult.recipe.target}:${laneIndex}`, {
        id: recipeId,
        result: lastRecipeResult
      });
      return draft;
    });
    if (!stagedResult) return;
    const detail = stagedResult.changedPaths.length
      ? `${stagedResult.changedPaths.length} exact field${stagedResult.changedPaths.length === 1 ? '' : 's'} changed.`
      : 'This slot already matched the recipe.';
    setStatus('warn', `${stagedResult.recipe.label} staged`, `${detail} Review before Apply.`);
  }

  function formatRecipeResult(result) {
    if (!result.changedPaths.length) return `${result.recipe.label}: already matched.`;
    const values = result.changedPaths.map((path) => {
      const value = path.split('.').reduce((cursor, segment) => cursor?.[segment], result.slot);
      return `${path} → ${String(value)}`;
    });
    return `Staged ${result.recipe.label}: ${values.join(' · ')}`;
  }

  function formatResponseSummary(ef) {
    const smoothing = Number(ef.smoothing);
    const mode = resolveEfModeName(ef);
    const feel =
      mode === 'GATE'
        ? 'Gate'
        : smoothing >= 0.4
          ? 'Tight'
          : smoothing <= 0.15
            ? 'Flowing'
            : 'Balanced';
    return `Response · ${feel} · smoothing ${formatNumberField(ef.smoothing, 2)} · attack ${formatNumberField(ef.attackMs, 0)} ms · release ${formatNumberField(ef.releaseMs, 0)} ms`;
  }

  function formatAmountSummary(ef) {
    if (ef.autoGain) {
      return `Amount · Adaptive · auto-gain target ${formatNumberField(ef.gainTarget, 0)}`;
    }
    const gain = Number(ef.gain);
    const feel = gain < 0.8 ? 'Subtle' : gain > 1.5 ? 'Strong' : 'Moderate';
    return `Amount · ${feel} · gain ×${formatNumberField(ef.gain, 2)}`;
  }

  function makeSelectedTuningEvidence(ef) {
    const evidence = document.createElement('section');
    evidence.className = 'selected-tuning-evidence';
    evidence.setAttribute('aria-label', 'Live tuning evidence');
    renderSelectedTuningEvidence(evidence, ef);
    return evidence;
  }

  function updateSelectedTuningEvidence() {
    const evidence = formContainer?.querySelector('.selected-tuning-evidence');
    const slot = slotState.slots[slotState.selected];
    if (!evidence || !slot) return;
    renderSelectedTuningEvidence(evidence, normalizeEf(slot));
  }

  function renderSelectedTuningEvidence(evidence, ef) {
    const telemetry = slotState.telemetry || {};
    const efIndex = Number(ef.index);
    const observedAt = Date.now();
    const active = efIndex >= 0 && Boolean(Number(telemetry.efStatus?.[efIndex]));
    if (active) efLastActiveAt.set(efIndex, observedAt);
    const recent =
      !active && efIndex >= 0 && observedAt - (efLastActiveAt.get(efIndex) ?? 0) <= 1500;
    const state = active ? 'Active' : recent ? 'Recent' : 'Inactive';
    const currentValue = efIndex >= 0 ? telemetry.envelopes?.[efIndex] : null;
    const output =
      telemetry.slotOutputs?.[slotState.selected] ?? telemetry.slots?.[slotState.selected];
    const contribution = Array.isArray(telemetry.slotContributions)
      ? telemetry.slotContributions.find((entry) => Number(entry?.index) === slotState.selected)
      : null;
    const parts = [
      efIndex >= 0 ? `EF ${efIndex + 1}` : 'No EF source',
      state,
      `Current ${currentValue ?? '—'}`,
      `Slot output ${output ?? '—'}`
    ];
    if (contribution && Number.isFinite(Number(contribution.ef))) {
      const delta = Number(contribution.ef);
      parts.push(`EF contribution ${delta > 0 ? '+' : ''}${delta}`);
    }
    if (resolveEfModeName(ef) === 'GATE') {
      parts.push(`Threshold ${formatNumberField(ef.gateThreshold, 0)}`);
    }
    evidence.textContent = parts.join(' · ');
    evidence.dataset.state = state.toLowerCase();
  }

  function makeDeckShortcut(combo, action) {
    const hint = document.createElement('p');
    hint.className = 'slot-hint deck-shortcut';
    hint.textContent = `Deck shortcut: ${combo} · ${action}`;
    return hint;
  }

  // Build a labeled `<select>` control with optional inline help.
  function makeSelect(
    labelText,
    options,
    current,
    onChange,
    { help, formatOptionLabel, describeOption, configPaths } = {}
  ) {
    const wrap = document.createElement('label');
    markDeviceConfigPaths(wrap, configPaths);
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
    if (describeOption) {
      const description = document.createElement('small');
      description.className = 'control-description';
      description.textContent = describeOption(current);
      select.addEventListener('change', () => {
        description.textContent = describeOption(select.value);
      });
      wrap.appendChild(description);
    }
    wrap.appendChild(select);
    return wrap;
  }

  // Build a numeric input with keyboard-friendly coarse/fine stepping.
  function makeNumber(labelText, current, min, max, step, onCommit, { help, configPaths } = {}) {
    const wrap = document.createElement('label');
    markDeviceConfigPaths(wrap, configPaths);
    wrap.appendChild(makeControlLabel(labelText, help));
    const input = document.createElement('input');
    input.type = 'number';
    if (min !== undefined && min !== null) input.min = String(min);
    if (max !== undefined && max !== null) input.max = String(max);
    if (step !== undefined && step !== null) input.step = String(step);
    input.value = current;
    input.setAttribute('aria-label', labelText);
    input.onchange = () => onCommit(Number(input.value));
    const coarseStep = step ?? 1;
    attachCoarseFine(input, coarseStep);
    wrap.appendChild(input);
    const continuous =
      getUiMode() === 'advanced' &&
      /attackMs|releaseMs|rmsWindowMs|TauMs|smoothing|gain$|lfo.*amount/.test(String(configPaths));
    if (continuous) {
      const slider = document.createElement('input');
      slider.type = 'range';
      slider.className = 'lab-continuous-control';
      slider.setAttribute('aria-label', `${labelText} slider`);
      const logarithmic = min > 0 && /Ms/.test(String(configPaths));
      slider.min = logarithmic ? '0' : String(min);
      slider.max = logarithmic ? '1000' : String(max);
      slider.step = logarithmic ? '1' : String(step);
      const position = (value) =>
        logarithmic
          ? (Math.log(Math.max(min, Number(value)) / min) / Math.log(max / min)) * 1000
          : Number(value);
      const sync = () => {
        slider.value = String(position(input.value));
        slider.setAttribute(
          'aria-valuetext',
          `${input.value}${/Ms/.test(String(configPaths)) ? ' ms' : ''}`
        );
      };
      sync();
      slider.addEventListener('input', () => {
        const raw = logarithmic
          ? min * Math.pow(max / min, Number(slider.value) / 1000)
          : Number(slider.value);
        const value = Math.max(
          min,
          Math.min(max, Number((Math.round(raw / step) * step).toFixed(6)))
        );
        input.value = String(value);
        sync();
        onCommit(value);
      });
      input.addEventListener('change', sync);
      const reset = () => {
        let path = wrap.dataset.stagedPath?.split(' ')[0];
        const lane = wrap.closest('[data-lfo-lane]')?.dataset.lfoLane;
        if (lane !== undefined) path = path?.replace('lfo.*', `lfo.${lane}`);
        const confirmed = path
          ?.split('.')
          .reduce((value, key) => value?.[key], runtime.getState().live);
        if (typeof confirmed !== 'number') return;
        input.value = String(confirmed);
        sync();
        onCommit(confirmed);
      };
      slider.addEventListener('dblclick', reset);
      input.addEventListener('dblclick', reset);
      const resetButton = document.createElement('button');
      resetButton.type = 'button';
      resetButton.className = 'reset-confirmed';
      resetButton.textContent = 'Reset to confirmed';
      resetButton.setAttribute('aria-label', `Reset ${labelText} to confirmed value`);
      resetButton.addEventListener('click', reset);
      wrap.append(slider, resetButton);
    }
    return wrap;
  }

  // Build a text input wrapper used for labels and SysEx templates.
  function makeText(labelText, current, placeholder, onCommit, { help, configPaths } = {}) {
    const wrap = document.createElement('label');
    markDeviceConfigPaths(wrap, configPaths);
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
  function makeToggle(labelText, current, onCommit, { help, configPaths } = {}) {
    const wrap = document.createElement('label');
    markDeviceConfigPaths(wrap, configPaths);
    wrap.className = 'toggle';
    const input = document.createElement('input');
    input.type = 'checkbox';
    input.checked = current;
    input.onchange = () => onCommit(input.checked);
    wrap.append(input, makeControlLabel(labelText, help));
    return wrap;
  }

  // Mark the exact device-schema leaves owned by a hand-built selected-slot
  // control. The Lab parity spec compares these markers with the negotiated
  // slot schema so a newly configurable firmware field cannot stay invisible.
  function markDeviceConfigPaths(element, paths) {
    const values = (Array.isArray(paths) ? paths : [paths]).filter(Boolean);
    if (values.length) {
      element.dataset.deviceConfigPath = values.join(' ');
      element.dataset.stagedPath = values
        .map((path) => path.replace('slots.*', `slots.${slotState.selected}`))
        .join(' ');
    }
  }

  function parkNoteDynamicsCard() {
    if (!noteDynamicsCard) return;
    noteDynamicsCard.hidden = true;
    if (noteDynamicsParking && noteDynamicsCard.parentElement !== noteDynamicsParking) {
      noteDynamicsParking.appendChild(noteDynamicsCard);
    }
  }

  function attachNoteDynamicsCard(container) {
    if (!noteDynamicsCard || !container) return;
    noteDynamicsCard.hidden = false;
    container.appendChild(noteDynamicsCard);
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
    badge.tabIndex = 0;
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

  // Stage one field in one of the two fixed slot-local LFO lanes.
  function stageSlotLfoField(index, laneIndex, key, value) {
    runtime.stage((draft) => {
      draft.slots = draft.slots || [];
      if (!draft.slots[index]) draft.slots[index] = {};
      const lanes = Array.isArray(draft.slots[index].lfo)
        ? draft.slots[index].lfo.map((lane) => ({ ...lane }))
        : [];
      while (lanes.length < 2) lanes.push({ enabled: false, mode: 4, amount: 0 });
      lanes[laneIndex] = { ...lanes[laneIndex], [key]: value };
      draft.slots[index].lfo = lanes.slice(0, 2);
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
    const freqLabel = freq === '—' ? '—' : `${freq} shaping`;
    return `${freqLabel} • Q ${q}`;
  }

  // Summarize EF dynamics-related tuning for the detail card.
  function formatEfDynamics(ef) {
    if (!ef) return '—';
    const modeName = resolveEfModeName(ef);
    const oversample = Number.isFinite(Number(ef.oversample)) ? Number(ef.oversample) : null;
    const smoothing = formatNumberField(ef.smoothing, 2);
    const pieces = [];
    if (modeName) pieces.push(formatEfModeLabel(modeName));
    if (oversample !== null) pieces.push(`Oversample ×${oversample}`);
    if (smoothing !== '—') pieces.push(`Smoothing ${smoothing}`);
    return pieces.length ? pieces.join(' • ') : '—';
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

  function normalizeSlotLfo(slot) {
    const lanes = Array.isArray(slot?.lfo) ? slot.lfo : [];
    return Array.from({ length: 2 }, (_, laneIndex) => {
      const source =
        lanes[laneIndex] && typeof lanes[laneIndex] === 'object' ? lanes[laneIndex] : {};
      const mode = Number(source.mode);
      const amount = Number(source.amount);
      return {
        enabled: Boolean(source.enabled),
        mode: Number.isFinite(mode) ? Math.max(0, Math.min(4, Math.round(mode))) : 4,
        amount: Number.isFinite(amount) ? Math.max(-100, Math.min(100, Math.round(amount))) : 0
      };
    });
  }

  function formatSlotLfoSummary(slot) {
    return normalizeSlotLfo(slot)
      .map((lane, laneIndex) => {
        if (!lane.enabled) return `L${laneIndex + 1} Off`;
        const mode = lfoModeOptions.find((option) => option.value === lane.mode)?.label ?? 'Mode';
        const amount = lane.amount > 0 ? `+${lane.amount}` : String(lane.amount);
        return `L${laneIndex + 1} ${mode} ${amount}%`;
      })
      .join(' · ');
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
      gain: 1,
      mode: 0,
      autoBaseline: true,
      autoGain: true,
      attackMs: 5,
      releaseMs: 20,
      rmsWindowMs: 50,
      baselineTauMs: 2000,
      gainTauMs: 3000,
      gateThreshold: 16,
      gateHysteresis: 4,
      activityThreshold: 4,
      gainTarget: 102,
      destination_mode: 'add_clamp'
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
    if (base.autoBaseline === undefined) base.autoBaseline = base.auto_baseline;
    if (base.autoGain === undefined) base.autoGain = base.auto_gain;
    if (base.attackMs === undefined) base.attackMs = base.attack_ms;
    if (base.releaseMs === undefined) base.releaseMs = base.release_ms;
    if (base.rmsWindowMs === undefined) base.rmsWindowMs = base.rms_ms;
    if (base.baselineTauMs === undefined) base.baselineTauMs = base.baseline_tau_ms;
    if (base.gainTauMs === undefined) base.gainTauMs = base.gain_tau_ms;
    if (base.gateThreshold === undefined) base.gateThreshold = base.gate_threshold;
    if (base.gateHysteresis === undefined) base.gateHysteresis = base.gate_hysteresis;
    if (base.activityThreshold === undefined) base.activityThreshold = base.activity_threshold;
    if (base.gainTarget === undefined) base.gainTarget = base.gain_target;
    if (base.destination_mode === undefined) {
      base.destination_mode =
        base.destinationMode ?? base.destination_mode_name ?? base.destinationModeName;
    }
    if (typeof base.mode === 'string') {
      const idx = efModeNames.indexOf(base.mode.toUpperCase());
      base.mode = idx >= 0 ? idx : defaults.mode;
    }
    base.mode = Number.isFinite(Number(base.mode))
      ? Math.max(0, Math.min(efModeNames.length - 1, Math.round(Number(base.mode))))
      : defaults.mode;
    base.autoBaseline =
      base.autoBaseline === undefined ? defaults.autoBaseline : Boolean(base.autoBaseline);
    base.autoGain = base.autoGain === undefined ? defaults.autoGain : Boolean(base.autoGain);
    base.attackMs = Number.isFinite(Number(base.attackMs))
      ? Math.max(1, Math.min(60000, Math.round(Number(base.attackMs))))
      : defaults.attackMs;
    base.releaseMs = Number.isFinite(Number(base.releaseMs))
      ? Math.max(1, Math.min(60000, Math.round(Number(base.releaseMs))))
      : defaults.releaseMs;
    base.rmsWindowMs = Number.isFinite(Number(base.rmsWindowMs))
      ? Math.max(1, Math.min(60000, Math.round(Number(base.rmsWindowMs))))
      : defaults.rmsWindowMs;
    base.baselineTauMs = Number.isFinite(Number(base.baselineTauMs))
      ? Math.max(1, Math.min(60000, Math.round(Number(base.baselineTauMs))))
      : defaults.baselineTauMs;
    base.gainTauMs = Number.isFinite(Number(base.gainTauMs))
      ? Math.max(1, Math.min(60000, Math.round(Number(base.gainTauMs))))
      : defaults.gainTauMs;
    base.gateThreshold = Number.isFinite(Number(base.gateThreshold))
      ? Math.max(0, Math.min(127, Math.round(Number(base.gateThreshold))))
      : defaults.gateThreshold;
    base.gateHysteresis = Number.isFinite(Number(base.gateHysteresis))
      ? Math.max(0, Math.min(127, Math.round(Number(base.gateHysteresis))))
      : defaults.gateHysteresis;
    base.activityThreshold = Number.isFinite(Number(base.activityThreshold))
      ? Math.max(0, Math.min(127, Math.round(Number(base.activityThreshold))))
      : defaults.activityThreshold;
    base.gainTarget = Number.isFinite(Number(base.gainTarget))
      ? Math.max(0, Math.min(127, Math.round(Number(base.gainTarget))))
      : defaults.gainTarget;
    base.destination_mode = resolveEfDestinationMode(base);
    base.index = index;
    return base;
  }

  function resolveEfDestinationMode(ef) {
    const raw =
      ef?.destination_mode ??
      ef?.destinationMode ??
      ef?.destination_mode_name ??
      ef?.destinationModeName;
    if (typeof raw === 'string') {
      const normalized = raw.toLowerCase();
      if (normalized === 'add') return 'add_clamp';
      if (efDestinationValues.includes(normalized)) return normalized;
    }
    const index = Number(raw);
    if (Number.isFinite(index)) {
      return efDestinationValues[Math.max(0, Math.min(efDestinationValues.length - 1, index))];
    }
    return 'add_clamp';
  }

  function resolveEfModeName(ef) {
    if (!ef) return null;
    if (typeof ef.mode === 'string') {
      const upper = ef.mode.toUpperCase();
      return efModeNames.includes(upper) ? upper : null;
    }
    const index = Number(ef.mode);
    if (Number.isFinite(index)) {
      return efModeNames[index] || null;
    }
    return null;
  }

  function formatEfModeLabel(mode) {
    switch (mode) {
      case 'PEAK':
        return 'Peak';
      case 'RMS':
        return 'RMS';
      case 'GATE':
        return 'Gate';
      case 'FOLLOWER':
        return 'Follower';
      default:
        return mode ?? 'Mode';
    }
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
    renderSlotEditor,
    focusControl,
    updateSignalPath
  };
}
