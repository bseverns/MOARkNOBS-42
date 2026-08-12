function cleanString(value, maxLength) {
  return typeof value === 'string' ? value.trim().slice(0, maxLength) : '';
}

function normalizeAppDisplayMetadata(value = {}) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) return null;
  const profileLabels = Array.isArray(value.profileLabels)
    ? value.profileLabels.slice(0, 16).map((label) => cleanString(label, 80))
    : [];
  const slots = Array.isArray(value.slots)
    ? value.slots.slice(0, 128).flatMap((slot) => {
        const index = Number(slot?.index);
        if (!Number.isInteger(index) || index < 0 || index > 127) return [];
        const label = cleanString(slot?.label, 80);
        const routeDescription = cleanString(slot?.routeDescription, 160);
        if (!label && !routeDescription) return [];
        return [{ index, label, routeDescription }];
      })
    : [];
  const activeProfile = Number(value.activeProfile);
  return {
    authority: 'advisory-browser-metadata',
    profileLabels,
    slots,
    activeProfile:
      Number.isInteger(activeProfile) && activeProfile >= 0 && activeProfile < 16
        ? activeProfile
        : null,
    observedAt: new Date().toISOString(),
  };
}

module.exports = { normalizeAppDisplayMetadata };
