export function normalizeConfigPath(path) {
  return String(path ?? '')
    .replace(/\[(\d+)\]/g, '.$1')
    .replace(/^\./, '');
}

export function pathsOverlap(left, right) {
  const a = normalizeConfigPath(left);
  const b = normalizeConfigPath(right);
  return a === b || a.startsWith(`${b}.`) || b.startsWith(`${a}.`);
}

export function updateStagedControlMarkers(root, changes = []) {
  root
    ?.querySelectorAll('[data-staged-path], .schema-control[data-device-config-path]')
    .forEach((node) => {
      const paths = String(node.dataset.stagedPath || node.dataset.deviceConfigPath || '')
        .split(/\s+/)
        .filter(Boolean);
      const changed = changes.some((change) =>
        paths.some((path) => pathsOverlap(path, change.path))
      );
      node.classList.toggle('is-staged', changed);
      let badge = node.querySelector(':scope > .staged-control-marker');
      if (changed && !badge) {
        badge = document.createElement('small');
        badge.className = 'staged-control-marker';
        badge.textContent = 'Staged';
        node.appendChild(badge);
      }
      if (badge) badge.hidden = !changed;
    });
}
