import { normalizeUIMode, persistUIMode } from '../state/ui_preferences.js';

const ALL_UI_MODES = ['stage', 'basic', 'advanced'];

function normalizeEditorTab(tab) {
  return tab === 'envelope' || tab === 'arg' ? tab : 'mapping';
}

function normalizeUtilityTab(tab) {
  return tab === 'diff' || tab === 'midi' || tab === 'scope' || tab === 'arp' || tab === 'lfo'
    ? tab
    : 'console';
}

function readAllowedModes(node) {
  const raw = node?.dataset?.uiModes;
  if (typeof raw === 'string' && raw.trim()) {
    const modes = raw
      .split(/[\s,]+/)
      .map((mode) => normalizeUIMode(mode))
      .filter((mode, index, values) => values.indexOf(mode) === index);
    if (modes.length) return modes;
  }
  if (node?.dataset?.uiTier === 'advanced') return ['advanced'];
  return ALL_UI_MODES;
}

function isModeAllowed(node, mode) {
  return readAllowedModes(node).includes(normalizeUIMode(mode));
}

function setModeVisibility(node, visible) {
  if (!node) return;
  node.classList.toggle('ui-tier-hidden', !visible);
  if (visible) {
    node.removeAttribute('aria-hidden');
  } else {
    node.setAttribute('aria-hidden', 'true');
  }
}

export function createUiModeController({
  docRoot = null,
  initialMode = 'basic',
  hints = {},
  elements = {},
  getSlotCount = () => 0,
  renderSlotEditor = () => {},
  setPerformerVisible = () => {},
  onModeChanged = () => {}
} = {}) {
  const {
    uiModeButtons = [],
    uiModeHint = null,
    uiModeNodes = [],
    advancedTierNodes = [],
    editorTabButtons = [],
    utilityTabButtons = [],
    utilityPanels = [],
    efAssignmentCard = null
  } = elements;

  let activeUiMode = normalizeUIMode(initialMode);
  let activeEditorTab = 'mapping';
  let activeUtilityTab = 'console';
  const managedNodes = Array.from(new Set([...uiModeNodes, ...advancedTierNodes]));

  function bind() {
    uiModeButtons.forEach((button) => {
      button.addEventListener('click', () => {
        setUIMode(button.dataset.uiModeBtn);
      });
    });
    editorTabButtons.forEach((button) => {
      button.addEventListener('click', () => {
        setEditorTab(button.dataset.editorTab);
      });
    });
    utilityTabButtons.forEach((button) => {
      button.addEventListener('click', () => {
        setUtilityTab(button.dataset.utilityTab);
      });
    });
  }

  function refreshModeNodes() {
    managedNodes.forEach((node) => {
      setModeVisibility(node, isModeAllowed(node, activeUiMode));
    });
  }

  function firstAvailableEditorTab() {
    const firstVisible = editorTabButtons.find((button) => isModeAllowed(button, activeUiMode));
    return normalizeEditorTab(firstVisible?.dataset?.editorTab);
  }

  function isEditorTabAllowed(tab) {
    return editorTabButtons.some(
      (button) =>
        isModeAllowed(button, activeUiMode) &&
        normalizeEditorTab(button.dataset.editorTab) === normalizeEditorTab(tab)
    );
  }

  function firstAvailableUtilityTab() {
    const firstVisible = utilityTabButtons.find((button) => isModeAllowed(button, activeUiMode));
    return normalizeUtilityTab(firstVisible?.dataset?.utilityTab);
  }

  function isUtilityTabAllowed(tab) {
    return utilityTabButtons.some(
      (button) =>
        isModeAllowed(button, activeUiMode) &&
        normalizeUtilityTab(button.dataset.utilityTab) === normalizeUtilityTab(tab)
    );
  }

  function setUIMode(mode, { persist = true } = {}) {
    activeUiMode = normalizeUIMode(mode);
    if (docRoot) docRoot.dataset.uiMode = activeUiMode;
    refreshModeNodes();

    uiModeButtons.forEach((button) => {
      const buttonMode = normalizeUIMode(button.dataset.uiModeBtn);
      button.setAttribute('aria-pressed', buttonMode === activeUiMode ? 'true' : 'false');
    });
    if (uiModeHint) {
      uiModeHint.textContent = hints[activeUiMode] || hints.basic || '';
    }
    if (persist) persistUIMode(activeUiMode);

    if (!isEditorTabAllowed(activeEditorTab)) activeEditorTab = firstAvailableEditorTab();
    if (!isUtilityTabAllowed(activeUtilityTab)) activeUtilityTab = firstAvailableUtilityTab();

    setPerformerVisible(true);
    refreshEditorTabs();
    refreshUtilityTabs();
    if (getSlotCount() > 0) renderSlotEditor();
    onModeChanged(activeUiMode);
  }

  function refreshEditorTabs() {
    const effectiveTab = isEditorTabAllowed(activeEditorTab)
      ? normalizeEditorTab(activeEditorTab)
      : firstAvailableEditorTab();
    activeEditorTab = effectiveTab;
    editorTabButtons.forEach((button) => {
      const buttonTab = normalizeEditorTab(button.dataset.editorTab);
      const allowed = isModeAllowed(button, activeUiMode);
      button.setAttribute('aria-pressed', allowed && buttonTab === effectiveTab ? 'true' : 'false');
    });
    if (efAssignmentCard) {
      efAssignmentCard.toggleAttribute(
        'hidden',
        !(isModeAllowed(efAssignmentCard, activeUiMode) && effectiveTab === 'envelope')
      );
    }
  }

  function refreshUtilityTabs() {
    const effectiveTab = isUtilityTabAllowed(activeUtilityTab)
      ? normalizeUtilityTab(activeUtilityTab)
      : firstAvailableUtilityTab();
    activeUtilityTab = effectiveTab;
    utilityTabButtons.forEach((button) => {
      const buttonTab = normalizeUtilityTab(button.dataset.utilityTab);
      const allowed = isModeAllowed(button, activeUiMode);
      button.setAttribute('aria-pressed', allowed && buttonTab === effectiveTab ? 'true' : 'false');
    });
    utilityPanels.forEach((panel) => {
      const panelTab = normalizeUtilityTab(panel.dataset.utilityPanel);
      const visible = isModeAllowed(panel, activeUiMode) && panelTab === effectiveTab;
      panel.classList.toggle('utility-panel-active', visible);
      panel.toggleAttribute('hidden', !visible);
    });
  }

  function setEditorTab(tab) {
    activeEditorTab = normalizeEditorTab(tab);
    refreshEditorTabs();
    if (getSlotCount() > 0) renderSlotEditor();
  }

  function setUtilityTab(tab) {
    activeUtilityTab = normalizeUtilityTab(tab);
    refreshUtilityTabs();
  }

  return {
    bind,
    setUIMode,
    setEditorTab,
    setUtilityTab,
    getUiMode: () => activeUiMode,
    getEditorTab: () => activeEditorTab
  };
}
