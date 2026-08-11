import { modulationIdentityForBadge } from './modulation_identity.js';

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

export function renderSlotModulationBadges(container, badges) {
  if (!container) return;
  const source = Array.isArray(badges) ? badges : [];
  container.replaceChildren(
    ...source.map((badge) => {
      const element = document.createElement('span');
      const identity = modulationIdentityForBadge(badge);
      element.className = 'modulation-badge';
      element.textContent = badge;
      if (identity) {
        element.dataset.modulationKind = identity.kind;
        element.dataset.modulationIndex = String(identity.index);
        element.style.setProperty('--modulation-color', identity.color);
      }
      return element;
    })
  );
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
