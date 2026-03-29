export const EF_FILTER_NAMES = [
  'LINEAR',
  'OPPOSITE_LINEAR',
  'EXPONENTIAL',
  'RANDOM',
  'LOWPASS',
  'HIGHPASS',
  'BANDPASS'
];

export const SLOT_TYPE_NAMES = [
  'OFF',
  'CC',
  'Note',
  'PitchBend',
  'ProgramChange',
  'Aftertouch',
  'ModWheel',
  'NRPN',
  'RPN',
  'SysEx'
];

export const ARG_METHOD_NAMES = [
  'PLUS',
  'MIN',
  'PECK',
  'SHAV',
  'SQAR',
  'BABS',
  'TABS',
  'MULT',
  'DIVI',
  'AVG',
  'XABS',
  'MAXX',
  'MINN',
  'XORR'
];

export const ARG_METHOD_DISPLAY = {
  PLUS: { rough: 'A + B', full: 'A + B' },
  MIN: { rough: 'A - B', full: 'A - B' },
  PECK: { rough: 'B - A', full: 'B - A' },
  SHAV: { rough: '(A - B) / k', full: '(A - B) / 10' },
  SQAR: { rough: 'sqrt(A^2 + B^2)', full: 'sqrt(A*A + B*B)' },
  BABS: { rough: 'A / |B|', full: 'A / abs(B)' },
  TABS: { rough: 'k·A / |B|', full: '(10 * A) / abs(B)' },
  MULT: { rough: 'A × B', full: '(A * B) / 127' },
  DIVI: { rough: 'A / (B + 1)', full: '(A * 127) / (B + 1)' },
  AVG: { rough: 'mean(A, B)', full: '(A + B) / 2' },
  XABS: { rough: '|A - B|', full: 'abs(A - B)' },
  MAXX: { rough: 'max(A, B)', full: 'max(A, B)' },
  MINN: { rough: 'min(A, B)', full: 'min(A, B)' },
  XORR: { rough: 'A xor B', full: 'A ^ B' }
};

export function formatArgMethodLabel(name) {
  const display = ARG_METHOD_DISPLAY[name];
  if (!display) return name;
  return `${name} · ${display.rough}`;
}

export function describeArgMethod(name) {
  const display = ARG_METHOD_DISPLAY[name];
  if (!display) return name;
  return `${name}: ${display.full}`;
}
