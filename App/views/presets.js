const SLOT_COUNT = 42;
const EF_COUNT = 6;

const clonePreset = (preset) => JSON.parse(JSON.stringify(preset));

const makeSlots = (mapper) =>
  Array.from({ length: SLOT_COUNT }, (_, idx) => {
    const base = {
      type: 'CC',
      midiChannel: ((idx % 6) + 1),
      data1: idx % 128,
      efIndex: idx % EF_COUNT,
      active: idx < 16,
      pot: idx % 3 === 0
    };
    const extra = mapper ? mapper(idx, { ...base }) : null;
    return extra ? { ...base, ...extra } : base;
  });

const makeEfSlots = (mapper) =>
  Array.from({ length: EF_COUNT }, (_, idx) => ({
    slots: [mapper ? mapper(idx) : (idx * 7) % SLOT_COUNT]
  }));

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

export const presets = [
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
  },
  {
    id: 'elektron-analog-rytm',
    label: 'Elektron Analog Rytm – TODO',
    load: null
  },
  {
    id: 'roland-mc707',
    label: 'Roland MC-707 – TODO',
    load: null
  }
];
