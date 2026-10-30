import { normalizeUIMode, persistUIMode } from '../state/ui_preferences.js';

const ALL_UI_MODES = ['stage', 'basic', 'advanced'];

function normalizeEditorTab(tab) {
  return tab === 'envelope' || tab === 'arg' || tab === 'lfo' ? tab : 'mapping';
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
  canChangeMode = () => true,
  onModeChanged = () => {}
} = {}) {
  const {
    uiModeButtons = [],
    uiModeHint = null,
    uiModeNodes = [],
    advancedTierNodes = [],
    editorTabButtons = [],
    efAssignmentCard = null
  } = elements;

  let activeUiMode = normalizeUIMode(initialMode);
  let activeEditorTab = 'mapping';
  const managedNodes = Array.from(new Set([...uiModeNodes, ...advancedTierNodes]));

  function initializeTabGroup(buttons, prefix, controlsForButton) {
    buttons.forEach((button, index) => {
      button.setAttribute('role', 'tab');
      button.id ||= `${prefix}-tab-${index}`;
      const controls = controlsForButton(button, index);
      if (controls) button.setAttribute('aria-controls', controls);
      button.addEventListener('keydown', (event) => {
        if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
        const visible = buttons.filter(
          (candidate) => isModeAllowed(candidate, activeUiMode) && !candidate.classList.contains('ui-tier-hidden')
        );
        if (!visible.length) return;
        const current = Math.max(0, visible.indexOf(button));
        const nextIndex =
          event.key === 'Home'
            ? 0
            : event.key === 'End'
              ? visible.length - 1
              : (current + (event.key === 'ArrowRight' ? 1 : -1) + visible.length) % visible.length;
        event.preventDefault();
        visible[nextIndex].focus();
        visible[nextIndex].click();
      });
    });
  }

  function bind() {
    initializeTabGroup(editorTabButtons, 'editor', () => 'form');

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


  function setUIMode(mode, { persist = true, preserveDraft = false } = {}) {
    const nextMode = normalizeUIMode(mode);
    if (nextMode !== activeUiMode && !preserveDraft && !canChangeMode(nextMode, activeUiMode)) return false;
    activeUiMode = nextMode;
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

    setPerformerVisible(activeUiMode === 'stage');
    refreshEditorTabs();
    if (getSlotCount() > 0) renderSlotEditor();
    onModeChanged(activeUiMode);
    return true;
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
      button.setAttribute('aria-selected', allowed && buttonTab === effectiveTab ? 'true' : 'false');
      button.tabIndex = allowed && buttonTab === effectiveTab ? 0 : -1;
    });
    if (efAssignmentCard) {
      efAssignmentCard.toggleAttribute(
        'hidden',
        !(isModeAllowed(efAssignmentCard, activeUiMode) && effectiveTab === 'envelope')
      );
    }
  }


  function setEditorTab(tab) {
    activeEditorTab = normalizeEditorTab(tab);
    refreshEditorTabs();
    if (getSlotCount() > 0) renderSlotEditor();
  }


  return {
    bind,
    setUIMode,
    setEditorTab,
    getUiMode: () => activeUiMode,
    getEditorTab: () => activeEditorTab
  };
}
