import { normalizeUIMode, persistUIMode } from '../state/ui_preferences.js';

function normalizeEditorTab(tab) {
  return tab === 'envelope' || tab === 'arg' ? tab : 'mapping';
}

function normalizeUtilityTab(tab) {
  return tab === 'service' ||
    tab === 'diff' ||
    tab === 'midi' ||
    tab === 'scope' ||
    tab === 'arp' ||
    tab === 'lfo'
    ? tab
    : 'console';
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
    advancedTierNodes = [],
    editorTabButtons = [],
    utilityTabButtons = [],
    utilityPanels = [],
    efAssignmentCard = null
  } = elements;

  let activeUiMode = normalizeUIMode(initialMode);
  let activeEditorTab = 'mapping';
  let activeUtilityTab = 'console';

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

  function setUIMode(mode, { persist = true } = {}) {
    activeUiMode = normalizeUIMode(mode);
    if (docRoot) docRoot.dataset.uiMode = activeUiMode;

    const hideAdvanced = activeUiMode !== 'advanced';
    advancedTierNodes.forEach((node) => {
      node.classList.toggle('ui-tier-hidden', hideAdvanced);
      if (hideAdvanced) {
        node.setAttribute('aria-hidden', 'true');
      } else {
        node.removeAttribute('aria-hidden');
      }
    });

    uiModeButtons.forEach((button) => {
      const buttonMode = normalizeUIMode(button.dataset.uiModeBtn);
      button.setAttribute('aria-pressed', buttonMode === activeUiMode ? 'true' : 'false');
    });
    if (uiModeHint) {
      uiModeHint.textContent = hints[activeUiMode] || hints.basic || '';
    }
    if (persist) persistUIMode(activeUiMode);

    if (activeUiMode !== 'advanced' && activeEditorTab !== 'mapping') {
      activeEditorTab = 'mapping';
    }
    if (activeUiMode === 'stage') {
      activeUtilityTab = 'console';
    } else if (
      activeUiMode !== 'advanced' &&
      (activeUtilityTab === 'scope' || activeUtilityTab === 'arp' || activeUtilityTab === 'lfo')
    ) {
      activeUtilityTab = 'console';
    }

    setPerformerVisible(true);
    refreshEditorTabs();
    refreshUtilityTabs();
    if (getSlotCount() > 0) renderSlotEditor();
    onModeChanged(activeUiMode);
  }

  function refreshEditorTabs() {
    const effectiveTab =
      activeUiMode === 'advanced' ? normalizeEditorTab(activeEditorTab) : 'mapping';
    activeEditorTab = effectiveTab;
    editorTabButtons.forEach((button) => {
      const buttonTab = normalizeEditorTab(button.dataset.editorTab);
      button.setAttribute('aria-pressed', buttonTab === effectiveTab ? 'true' : 'false');
    });
    if (efAssignmentCard) {
      efAssignmentCard.toggleAttribute(
        'hidden',
        !(activeUiMode === 'advanced' && effectiveTab === 'envelope')
      );
    }
  }

  function refreshUtilityTabs() {
    const effectiveTab =
      activeUiMode === 'advanced'
        ? normalizeUtilityTab(activeUtilityTab)
        : normalizeUtilityTab(activeUtilityTab === 'scope' ? 'console' : activeUtilityTab);
    activeUtilityTab = effectiveTab;
    utilityTabButtons.forEach((button) => {
      const buttonTab = normalizeUtilityTab(button.dataset.utilityTab);
      button.setAttribute('aria-pressed', buttonTab === effectiveTab ? 'true' : 'false');
    });
    utilityPanels.forEach((panel) => {
      const panelTab = normalizeUtilityTab(panel.dataset.utilityPanel);
      panel.classList.toggle('utility-panel-active', panelTab === effectiveTab);
      panel.toggleAttribute('hidden', panelTab !== effectiveTab);
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
