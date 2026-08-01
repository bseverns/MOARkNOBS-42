export function describeSlotModulation(slot) {
  const badges = [];
  const efIndex = Number(slot?.efIndex ?? slot?.ef_index ?? slot?.ef?.index);
  if (Number.isFinite(efIndex) && efIndex >= 0) badges.push(`E${Math.round(efIndex) + 1}`);
  if (slot?.arg?.enabled) badges.push('A');
  const lanes = Array.isArray(slot?.lfo) ? slot.lfo : [];
  if (lanes[0]?.enabled) badges.push('L1');
  if (lanes[1]?.enabled) badges.push('L2');
  return badges;
}

export function formatSlotModulationTitle(badges) {
  if (!badges.length) return 'No configured slot modulation';
  return badges
    .map((badge) => {
      if (badge === 'A') return 'ARG enabled';
      if (badge.startsWith('E')) return `Envelope follower ${badge.slice(1)}`;
      return `Fixed ${badge.replace('L', 'LFO ')} lane enabled`;
    })
    .join(' · ');
}
