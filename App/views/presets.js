const SLOT_COUNT = 42;
const EF_COUNT = 6;

// Clone static preset objects so loading one preset never mutates the source template.
const clonePreset = (preset) => JSON.parse(JSON.stringify(preset));

// Generate a full 42-slot preset skeleton and let the caller override each slot.
const makeSlots = (mapper) =>
  Array.from({ length: SLOT_COUNT }, (_, idx) => {
    const base = {
      type: 'CC',
      midiChannel: (idx % 6) + 1,
      data1: idx % 128,
      efIndex: idx % EF_COUNT,
      active: idx < 16,
      pot: idx % 3 === 0
    };
    const extra = mapper ? mapper(idx, { ...base }) : null;
    return extra ? { ...base, ...extra } : base;
  });

// Generate the envelope-follower assignment block used by demo presets.
const makeEfSlots = (mapper) =>
  Array.from({ length: EF_COUNT }, (_, idx) => ({
    slots: [mapper ? mapper(idx) : (idx * 7) % SLOT_COUNT]
  }));

// Wrap a JSON preset file in the async loader shape expected by the preset picker.
const fetchPreset = (path) => async () => {
  const response = await fetch(path);
  if (!response.ok) {
    throw new Error(`Preset fetch failed: ${response.status} ${response.statusText}`);
  }
  return response.json();
};

const aeModularSketch = {
  slots: makeSlots((idx) => {
    if (idx % 6 === 0) {
      return {
        type: 'NRPN',
        midiChannel: 1,
        data1: (32 + idx) % 128,
        label: `SEQ ${Math.floor(idx / 6) + 1}`,
        active: true,
        pot: true
      };
    }
    if (idx % 4 === 0) {
      return {
        type: 'Note',
        midiChannel: 2,
        data1: (60 + idx) % 128,
        active: true
      };
    }
    return {
      type: 'CC',
      midiChannel: (idx % 3) + 1,
      data1: (idx * 5) % 128,
      active: idx % 5 !== 4
    };
  }),
  efSlots: makeEfSlots((idx) => (idx * 8) % SLOT_COUNT),
  filter: { type: 'LOWPASS', freq: 520, q: 1.4 },
  arg: { method: 'PLUS', a: 1.8, b: 0.6, enable: true },
  led: {
    brightness: 64,
    color: '#39FF9F'
  },
  envelopeMode: 'EXPONENTIAL'
};

const demoProfileA = {
  slots: makeSlots((idx) => ({
    type: idx % 6 === 0 ? 'Note' : 'CC',
    midiChannel: (idx % 4) + 1,
    data1: idx % 6 === 0 ? (48 + idx) % 96 : (10 + idx * 3) % 128,
    active: idx < 24,
    label: idx < 6 ? `DEMO_A ${idx + 1}` : undefined
  })),
  efSlots: makeEfSlots((idx) => (idx * 6) % SLOT_COUNT),
  filter: { type: 'LOWPASS', freq: 680, q: 1.2 },
  arg: { method: 'MAXX', a: 1.6, b: 0.45, enable: true },
  led: {
    brightness: 78,
    color: '#27F3B6'
  },
  envelopeMode: 'EXPONENTIAL'
};

const demoProfileB = {
  slots: makeSlots((idx) => ({
    type: idx % 5 === 0 ? 'NRPN' : 'CC',
    midiChannel: ((idx + 2) % 6) + 1,
    data1: idx % 5 === 0 ? (64 + idx) % 128 : (idx * 5) % 128,
    active: idx % 3 !== 0,
    label: idx < 6 ? `DEMO_B ${idx + 1}` : undefined
  })),
  efSlots: makeEfSlots((idx) => (idx * 7 + 3) % SLOT_COUNT),
  filter: { type: 'BANDPASS', freq: 920, q: 1.85 },
  arg: { method: 'XABS', a: 1.25, b: 0.85, enable: true },
  led: {
    brightness: 58,
    color: '#FF7A3D'
  },
  envelopeMode: 'LINEAR'
};

export const presets = [
  {
    id: 'demo-profile-a',
    label: 'DEMO_A - Reactive Stack',
    load: async () => clonePreset(demoProfileA)
  },
  {
    id: 'demo-profile-b',
    label: 'DEMO_B - Clock Contrast',
    load: async () => clonePreset(demoProfileB)
  },
  {
    id: 'korg-minilogue-init',
    label: 'Korg Minilogue XD – Layer Launch',
    load: fetchPreset('./presets/korg/minilogue-init.json')
  },
  {
    id: 'ae-modular-sketch',
    label: 'AE Modular – Probability Sketch',
    load: async () => clonePreset(aeModularSketch)
  },
  {
    id: 'akai-mpc-performance',
    label: 'Akai MPC – Performance Grid',
    load: fetchPreset('./presets/akai/mpc-performance.json')
  }
];
