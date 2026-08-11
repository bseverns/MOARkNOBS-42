const EF_RGB = [
  [45, 226, 230],
  [255, 198, 88],
  [128, 224, 108],
  [190, 130, 255],
  [255, 133, 82],
  [100, 164, 255]
];

const LFO_RGB = [
  [255, 95, 150],
  [232, 238, 255]
];

function paletteFor(kind) {
  return kind === 'lfo' ? LFO_RGB : EF_RGB;
}

// Keep each modulation source visually stable everywhere it appears. The
// palettes wrap so a future manifest can advertise more lanes without breaking
// the UI while still giving today's six EFs and two LFOs distinct identities.
export function modulationIdentity(kind, index = 0) {
  const normalizedKind = kind === 'lfo' ? 'lfo' : 'ef';
  const palette = paletteFor(normalizedKind);
  const numericIndex = Number.isFinite(Number(index)) ? Math.max(0, Math.floor(Number(index))) : 0;
  const rgb = palette[numericIndex % palette.length];
  return {
    kind: normalizedKind,
    index: numericIndex,
    key: `${normalizedKind}-${numericIndex + 1}`,
    label: `${normalizedKind === 'lfo' ? 'LFO' : 'EF'} ${numericIndex + 1}`,
    color: `rgb(${rgb.join(', ')})`,
    rgba(alpha = 1) {
      const opacity = Number.isFinite(Number(alpha))
        ? Math.min(1, Math.max(0, Number(alpha)))
        : 1;
      return `rgba(${rgb.join(', ')}, ${opacity})`;
    }
  };
}

export function modulationIdentityForBadge(badge) {
  const match = String(badge ?? '').trim().match(/^(E|L)(\d+)$/i);
  if (!match) return null;
  const index = Number(match[2]) - 1;
  if (index < 0) return null;
  return modulationIdentity(match[1].toUpperCase() === 'L' ? 'lfo' : 'ef', index);
}

export function applyModulationIdentity(element, kind, index) {
  if (!element) return null;
  const identity = modulationIdentity(kind, index);
  element.dataset.modulationKind = identity.kind;
  element.dataset.modulationIndex = String(identity.index);
  element.style.setProperty('--modulation-color', identity.color);
  return identity;
}
