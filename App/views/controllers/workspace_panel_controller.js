const normalizeModes = (modes = []) =>
  Array.from(new Set(modes.map((mode) => String(mode))));

export function createWorkspacePanelController({
  document,
  picker,
  registry = [],
  initialMode = 'basic',
  onVisibilityChanged = () => {}
} = {}) {
  const entries = registry.map((descriptor) => ({
    ...descriptor,
    modes: normalizeModes(descriptor.modes),
    node: document?.querySelector(descriptor.selector) ?? null,
    button: null
  }));
  let activeMode = initialMode;
  const initializedOpen = new Set();

  function allowed(entry) {
    return entry.modes.includes(activeMode);
  }

  function setOpen(entry, open, { focus = false, updateUrl = true } = {}) {
    if (!entry?.node || (open && !allowed(entry))) return false;
    entry.node.hidden = !open;
    entry.node.dataset.workspaceOpen = String(open);
    entry.button?.setAttribute('aria-pressed', String(open));
    entry.button?.classList.toggle('active', open);
    entry.node.dispatchEvent(
      new CustomEvent('workspace-panel-visibility', { detail: { open, id: entry.id } })
    );
    onVisibilityChanged(entry, open);
    refreshGroupVisibility();
    if (open && focus) {
      const heading = entry.node.querySelector('h2, h3, summary, [role="heading"]') ?? entry.node;
      heading.tabIndex = -1;
      heading.focus({ preventScroll: true });
      entry.node.scrollIntoView?.({ block: 'start' });
    }
    if (updateUrl) syncUrl();
    return true;
  }

  function refreshGroupVisibility() {
    picker?.querySelectorAll('[data-workspace-tool-group]').forEach((group) => {
      group.hidden = !Array.from(group.querySelectorAll('[data-workspace-tool]')).some(
        (button) => !button.hidden
      );
    });
    document?.querySelectorAll('[data-workspace-group]').forEach((group) => {
      const hasOpenPanel = entries.some(
        (entry) => entry.group === group.dataset.workspaceGroup && entry.node && !entry.node.hidden
      );
      group.hidden = !hasOpenPanel;
      group.classList.toggle('utility-panel-active', hasOpenPanel);
    });
  }

  function syncUrl() {
    if (typeof window === 'undefined' || !window.history?.replaceState) return;
    const url = new URL(window.location.href);
    url.searchParams.set('mode', activeMode);
    const openIds = entries.filter((entry) => entry.node && !entry.node.hidden).map((entry) => entry.id);
    if (openIds.length) url.searchParams.set('panel', openIds.join(','));
    else url.searchParams.delete('panel');
    window.history.replaceState(null, '', url);
  }

  function renderPicker() {
    if (!picker) return;
    picker.innerHTML = '';
    const groups = [];
    entries.forEach((entry) => {
      if (!groups.includes(entry.group)) groups.push(entry.group);
    });
    groups.forEach((groupName) => {
      const groupEntries = entries.filter((entry) => entry.group === groupName);
      const fieldset = document.createElement('fieldset');
      fieldset.className = 'workspace-tool-group';
      fieldset.dataset.workspaceToolGroup = groupName;
      fieldset.setAttribute('role', 'group');
      fieldset.setAttribute('aria-label', `${groupName} tools`);
      const legend = document.createElement('legend');
      legend.textContent = groupName;
      fieldset.append(legend);
      groupEntries.forEach((entry) => {
        const button = document.createElement('button');
        button.type = 'button';
        button.dataset.workspaceTool = entry.id;
        button.textContent = entry.label;
        button.hidden = !allowed(entry);
        button.disabled = !entry.node;
        button.setAttribute('aria-pressed', String(Boolean(entry.node && !entry.node.hidden)));
        if (entry.node) {
          entry.node.id ||= `workspace-panel-${entry.id}`;
          entry.node.dataset.workspacePanel = entry.id;
          button.setAttribute('aria-controls', entry.node.id);
        }
        button.addEventListener('click', () => toggle(entry.id, { focus: true }));
        entry.button = button;
        fieldset.append(button);
      });
      picker.append(fieldset);
    });
    refreshGroupVisibility();
  }

  function closeAll(options = {}) {
    entries.forEach((entry) => {
      if (entry.node && !entry.node.hidden) setOpen(entry, false, { ...options, updateUrl: false });
    });
    if (options.updateUrl !== false) syncUrl();
  }

  function setMode(mode, { openDeepLink = false } = {}) {
    activeMode = String(mode);
    if (openDeepLink) {
      closeAll({ updateUrl: false });
    } else {
      entries
        .filter((entry) => entry.node && !entry.node.hidden && !allowed(entry))
        .forEach((entry) => setOpen(entry, false, { updateUrl: false }));
    }
    renderPicker();
    if (openDeepLink && typeof window !== 'undefined') {
      const params = new URLSearchParams(window.location.search);
      const requested = params.get('panel') ?? params.get('panels') ?? '';
      requested
        .split(',')
        .map((id) => id.trim())
        .filter(Boolean)
        .forEach((id) => open(id, { updateUrl: false }));
    }
    entries
      .filter((entry) => entry.initialOpen && allowed(entry) && !initializedOpen.has(entry.id))
      .forEach((entry) => {
        initializedOpen.add(entry.id);
        open(entry.id, { updateUrl: false });
      });
    refreshGroupVisibility();
    syncUrl();
  }

  function open(id, options = {}) {
    return setOpen(entries.find((entry) => entry.id === id), true, options);
  }

  function close(id, options = {}) {
    return setOpen(entries.find((entry) => entry.id === id), false, options);
  }

  function toggle(id, options = {}) {
    const entry = entries.find((candidate) => candidate.id === id);
    if (!entry?.node) return false;
    return setOpen(entry, entry.node.hidden, options);
  }

  entries.forEach((entry) => {
    if (!entry.node) return;
    const mount = entry.mount ? document?.querySelector(entry.mount) : null;
    if (mount && entry.node.parentElement !== mount) mount.append(entry.node);
    entry.node.dataset.workspacePanel = entry.id;
    entry.node.dataset.workspaceOpen = 'false';
    entry.node.hidden = true;
  });
  renderPicker();

  return {
    open,
    close,
    closeAll,
    toggle,
    setMode,
    getMode: () => activeMode,
    getOpenPanelIds: () => entries.filter((entry) => entry.node && !entry.node.hidden).map((entry) => entry.id)
  };
}
