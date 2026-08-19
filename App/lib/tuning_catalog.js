// Musician-facing presentation metadata for existing configuration values.
// config_schema.json remains the validation contract; firmware remains device truth.

export const EF_FILTER_PRESENTATION = Object.freeze({
  LINEAR: {
    musicalLabel: 'Literal',
    technicalLabel: 'LINEAR',
    explanation: 'The most direct response and the clearest neutral reference.',
    beginner: true
  },
  OPPOSITE_LINEAR: {
    musicalLabel: 'Inverted',
    technicalLabel: 'OPPOSITE_LINEAR',
    explanation: 'The same literal response with louder motion pulling downward.',
    beginner: false
  },
  EXPONENTIAL: {
    musicalLabel: 'Punchy',
    technicalLabel: 'EXPONENTIAL',
    explanation: 'Emphasizes attacks and makes transient playing feel more dramatic.',
    beginner: true
  },
  RANDOM: {
    musicalLabel: 'Unruly',
    technicalLabel: 'RANDOM',
    explanation: 'Adds deliberate generative instability for experimental patches.',
    beginner: false
  },
  LOWPASS: {
    musicalLabel: 'Smooth',
    technicalLabel: 'LOWPASS',
    explanation: 'Calms fizzy motion and makes reactive changes feel steadier.',
    beginner: true
  },
  HIGHPASS: {
    musicalLabel: 'Attack-focused',
    technicalLabel: 'HIGHPASS',
    explanation: 'Responds more to change and attacks than to steady level.',
    beginner: false
  },
  BANDPASS: {
    musicalLabel: 'Focused',
    technicalLabel: 'BANDPASS',
    explanation: 'Selects a narrower, more characterful slice of motion.',
    beginner: false
  }
});

export const ARG_METHOD_PRESENTATION = Object.freeze({
  PLUS: {
    musicalLabel: 'Add Together',
    technicalLabel: 'PLUS',
    explanation: 'Both followers reinforce each other.',
    formula: 'A + B',
    beginner: true
  },
  MIN: {
    musicalLabel: 'A Minus B',
    technicalLabel: 'MIN',
    explanation: 'Follower B pulls down follower A.',
    formula: 'A - B',
    beginner: false
  },
  PECK: {
    musicalLabel: 'B Minus A',
    technicalLabel: 'PECK',
    explanation: 'Follower A pulls down follower B.',
    formula: 'B - A',
    beginner: false
  },
  SHAV: {
    musicalLabel: 'Gentle Difference',
    technicalLabel: 'SHAV',
    explanation: 'A restrained version of A minus B.',
    formula: '(A - B) / 10',
    beginner: false
  },
  SQAR: {
    musicalLabel: 'Combined Energy',
    technicalLabel: 'SQAR',
    explanation: 'Builds a fuller magnitude from both followers.',
    formula: 'sqrt(A*A + B*B)',
    beginner: false
  },
  BABS: {
    musicalLabel: 'A Relative to B',
    technicalLabel: 'BABS',
    explanation: 'Compares A against the absolute level of B.',
    formula: 'A / abs(B)',
    beginner: false
  },
  TABS: {
    musicalLabel: 'Strong A Relative to B',
    technicalLabel: 'TABS',
    explanation: 'A more aggressive relative comparison.',
    formula: '(10 * A) / abs(B)',
    beginner: false
  },
  MULT: {
    musicalLabel: 'Interaction',
    technicalLabel: 'MULT',
    explanation: 'Both followers multiply into one another.',
    formula: '(A * B) / 127',
    beginner: false
  },
  DIVI: {
    musicalLabel: 'Ratio',
    technicalLabel: 'DIVI',
    explanation: 'Creates unstable comparative motion from A divided by B.',
    formula: '(A * 127) / (B + 1)',
    beginner: false
  },
  AVG: {
    musicalLabel: 'Average Together',
    technicalLabel: 'AVG',
    explanation: 'Both followers negotiate a smoother middle path.',
    formula: '(A + B) / 2',
    beginner: true
  },
  XABS: {
    musicalLabel: 'Difference',
    technicalLabel: 'XABS',
    explanation: 'The output grows when the followers disagree.',
    formula: 'abs(A - B)',
    beginner: true
  },
  MAXX: {
    musicalLabel: 'Strongest Wins',
    technicalLabel: 'MAXX',
    explanation: 'Whichever follower is stronger takes over.',
    formula: 'max(A, B)',
    beginner: true
  },
  MINN: {
    musicalLabel: 'Quietest Wins',
    technicalLabel: 'MINN',
    explanation: 'Whichever follower is quieter sets the result.',
    formula: 'min(A, B)',
    beginner: false
  },
  XORR: {
    musicalLabel: 'Glitch',
    technicalLabel: 'XORR',
    explanation: 'Bitwise interaction creates deliberate digital disorder.',
    formula: 'A ^ B',
    beginner: false
  }
});

export const EF_DESTINATION_PRESENTATION = Object.freeze({
  add_clamp: {
    musicalLabel: 'Louder → more',
    technicalLabel: 'ADD_CLAMP',
    explanation: 'Add the follower to the knob value and clamp at the MIDI limit.'
  },
  subtract: {
    musicalLabel: 'Louder → less',
    technicalLabel: 'SUBTRACT',
    explanation: 'Subtract the follower from the knob value.'
  },
  replace: {
    musicalLabel: 'Signal replaces value',
    technicalLabel: 'REPLACE',
    explanation: 'Use the follower value instead of the knob value.'
  },
  scale: {
    musicalLabel: 'Signal scales value',
    technicalLabel: 'SCALE',
    explanation: 'Use the follower as a multiplier for the knob value.'
  },
  centered: {
    musicalLabel: 'Move around knob center',
    technicalLabel: 'CENTERED',
    explanation: 'Treat the follower as an offset around MIDI center 64.'
  }
});

export const EF_FILTER_NAMES = Object.freeze(Object.keys(EF_FILTER_PRESENTATION));
export const ARG_METHOD_NAMES = Object.freeze(Object.keys(ARG_METHOD_PRESENTATION));

function formatPresentationLabel(catalog, value) {
  const entry = catalog[value];
  return entry ? `${entry.musicalLabel} · ${entry.technicalLabel}` : value;
}

export function formatEfFilterLabel(value) {
  return formatPresentationLabel(EF_FILTER_PRESENTATION, value);
}

export function describeEfFilter(value) {
  return EF_FILTER_PRESENTATION[value]?.explanation ?? value;
}

export function formatArgMethodLabel(value) {
  return formatPresentationLabel(ARG_METHOD_PRESENTATION, value);
}

export function describeArgMethod(value) {
  const entry = ARG_METHOD_PRESENTATION[value];
  return entry ? `${entry.explanation} Technical formula: ${entry.formula}.` : value;
}

export function formatEfDestinationLabel(value) {
  return formatPresentationLabel(EF_DESTINATION_PRESENTATION, value);
}

export function describeDeviceConfigPatch(patch, previousConfig = null, liveConfig = null) {
  const entry = Array.isArray(patch?.slots) ? patch.slots[0] : null;
  if (!entry || typeof entry !== 'object') return 'Configuration updated';
  const rawIndex = Number(entry.index ?? entry.slot ?? entry.slot_index);
  const slotLabel = Number.isInteger(rawIndex) && rawIndex >= 0 ? `Slot ${rawIndex + 1}` : 'Slot';
  const before = Number.isInteger(rawIndex) ? previousConfig?.slots?.[rawIndex] : null;
  const after = Number.isInteger(rawIndex) ? liveConfig?.slots?.[rawIndex] : null;
  const comparesStates = Boolean(before && after);
  const details = [];
  const oversample = after?.ef?.oversample ?? entry.ef?.oversample;
  if (
    Number.isFinite(Number(oversample)) &&
    (!comparesStates || Number(before?.ef?.oversample) !== Number(oversample))
  ) {
    details.push(`EF oversampling → ${Number(oversample)}×`);
  }
  const argEnabled = after?.arg?.enabled ?? entry.arg?.enabled;
  if (
    argEnabled !== undefined &&
    (!comparesStates || Boolean(before?.arg?.enabled) !== Boolean(argEnabled))
  ) {
    details.push(`ARG → ${argEnabled ? 'On' : 'Off'}`);
  }
  const lane = after?.lfo?.[0] ?? (Array.isArray(entry.lfo) ? entry.lfo[0] : null);
  if (
    lane &&
    (!comparesStates || JSON.stringify(before?.lfo?.[0] ?? null) !== JSON.stringify(lane))
  ) {
    const modeNames = ['Add', 'Subtract', 'Replace', 'Scale', 'Centered'];
    details.push(
      `LFO 1 → ${lane.enabled ? 'On' : 'Off'} · ${modeNames[Number(lane.mode)] ?? `Mode ${lane.mode}`} · ${Number(lane.amount) || 0}%`
    );
  }
  return [slotLabel, ...(details.length ? details : ['Configuration updated'])].join(' · ');
}

export const SLOT_TUNING_RECIPES = Object.freeze([
  {
    id: 'ef-neutral',
    target: 'ef',
    label: 'Clean / Neutral',
    explanation: 'Reset response shaping to the firmware defaults without touching source, calibration, gain, or direction.',
    patch: {
      filter_name: 'LINEAR',
      filter_index: 0,
      frequency: 1000,
      q: 0.707,
      oversample: 4,
      smoothing: 0.2,
      mode: 0,
      attackMs: 5,
      releaseMs: 20
    }
  },
  {
    id: 'ef-smooth',
    target: 'ef',
    label: 'Smooth',
    explanation: 'A calmer LOWPASS starting point with more sampling and slower EWMA response.',
    patch: {
      filter_name: 'LOWPASS',
      filter_index: 4,
      frequency: 1000,
      q: 0.707,
      oversample: 8,
      smoothing: 0.12,
      mode: 0,
      attackMs: 5,
      releaseMs: 20
    }
  },
  {
    id: 'ef-punchy',
    target: 'ef',
    label: 'Punchy',
    explanation: 'An EXPONENTIAL response with a more immediate smoothing weight.',
    patch: {
      filter_name: 'EXPONENTIAL',
      filter_index: 2,
      frequency: 1000,
      q: 0.707,
      oversample: 4,
      smoothing: 0.45,
      mode: 0,
      attackMs: 5,
      releaseMs: 20
    }
  },
  {
    id: 'ef-gate',
    target: 'ef',
    label: 'Gate',
    explanation: 'A thresholded on/off response using the firmware gate defaults.',
    patch: {
      filter_name: 'LINEAR',
      filter_index: 0,
      frequency: 1000,
      q: 0.707,
      oversample: 4,
      smoothing: 1,
      mode: 2,
      gateThreshold: 16,
      gateHysteresis: 4
    }
  },
  {
    id: 'ef-experimental',
    target: 'ef',
    label: 'Experimental',
    explanation: 'A deliberately active RANDOM starting point for patch-lab exploration.',
    patch: {
      filter_name: 'RANDOM',
      filter_index: 3,
      frequency: 3500,
      q: 2.5,
      oversample: 2,
      smoothing: 0.65,
      mode: 0,
      attackMs: 5,
      releaseMs: 20
    }
  },
  {
    id: 'arg-strongest',
    target: 'arg',
    label: 'Strongest Wins',
    explanation: 'Enable ARG and let the stronger source take over.',
    patch: { enabled: true, method: 11, method_name: 'MAXX' }
  },
  {
    id: 'arg-average',
    target: 'arg',
    label: 'Average Together',
    explanation: 'Enable ARG and blend both sources toward their average.',
    patch: { enabled: true, method: 9, method_name: 'AVG' }
  },
  {
    id: 'arg-difference',
    target: 'arg',
    label: 'Difference',
    explanation: 'Enable ARG and use the absolute difference between sources.',
    patch: { enabled: true, method: 10, method_name: 'XABS' }
  },
  {
    id: 'arg-interaction',
    target: 'arg',
    label: 'Interaction / Multiply',
    explanation: 'Enable ARG and multiply the two source levels.',
    patch: { enabled: true, method: 7, method_name: 'MULT' }
  },
  {
    id: 'lfo-subtle-centered',
    target: 'lfo',
    label: 'Subtle Centered Motion',
    explanation: 'Enable the selected fixed lane around the knob center at 25%.',
    patch: { enabled: true, mode: 4, amount: 25 }
  },
  {
    id: 'lfo-wide-centered',
    target: 'lfo',
    label: 'Wide Centered Motion',
    explanation: 'Enable the selected fixed lane around the knob center at 100%.',
    patch: { enabled: true, mode: 4, amount: 100 }
  }
]);

const RECIPES_BY_ID = new Map(SLOT_TUNING_RECIPES.map((recipe) => [recipe.id, recipe]));

function mergePatch(source, patch, prefix, changedPaths) {
  const next = { ...(source && typeof source === 'object' ? source : {}) };
  for (const [key, value] of Object.entries(patch)) {
    const path = prefix ? `${prefix}.${key}` : key;
    if (value && typeof value === 'object' && !Array.isArray(value)) {
      next[key] = mergePatch(next[key], value, path, changedPaths);
    } else {
      if (next[key] !== value) changedPaths.push(path);
      next[key] = value;
    }
  }
  return next;
}

export function applySlotTuningRecipe(slot, recipeId, { laneIndex = 0 } = {}) {
  const recipe = RECIPES_BY_ID.get(recipeId);
  if (!recipe) throw new Error(`Unknown tuning recipe: ${recipeId}`);
  const source = slot && typeof slot === 'object' ? slot : {};
  const changedPaths = [];
  let nextSlot = { ...source };

  if (recipe.target === 'lfo') {
    const index = Math.max(0, Math.min(1, Math.round(Number(laneIndex) || 0)));
    const lanes = Array.from({ length: 2 }, (_, candidate) => ({
      enabled: false,
      mode: 4,
      amount: 0,
      ...(Array.isArray(source.lfo) ? source.lfo[candidate] : {})
    }));
    lanes[index] = mergePatch(lanes[index], recipe.patch, `lfo.${index}`, changedPaths);
    nextSlot.lfo = lanes;
  } else {
    nextSlot[recipe.target] = mergePatch(
      source[recipe.target],
      recipe.patch,
      recipe.target,
      changedPaths
    );
  }

  return {
    slot: nextSlot,
    recipe,
    changedPaths
  };
}
