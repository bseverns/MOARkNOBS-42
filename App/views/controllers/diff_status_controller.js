function summarizeDiffValue(value) {
  if (value === undefined) return 'undefined';
  if (value === null) return 'null';
  if (typeof value === 'string') return truncateDiffText(JSON.stringify(value), 150);
  if (typeof value === 'number' || typeof value === 'boolean') return String(value);
  if (Array.isArray(value)) {
    return `Array(${value.length}) ${truncateDiffText(stringifyDiffValue(value), 150)}`;
  }
  if (typeof value === 'object') {
    const keys = Object.keys(value);
    const preview = keys.slice(0, 5).join(', ');
    const suffix = keys.length > 5 ? ', …' : '';
    return `Object{${preview}${suffix}} ${truncateDiffText(stringifyDiffValue(value), 140)}`;
  }
  return truncateDiffText(String(value), 150);
}

function stringifyDiffValue(value) {
  try {
    return JSON.stringify(value);
  } catch {
    return '[unserializable]';
  }
}

function truncateDiffText(text, maxLength) {
  if (typeof text !== 'string') return '';
  if (text.length <= maxLength) return text;
  return `${text.slice(0, Math.max(0, maxLength - 1))}…`;
}

export function createDiffStatusController({
  runtime,
  elements = {},
  onDirtyChanged = () => {}
} = {}) {
  const {
    statusEl = null,
    statusLabel = null,
    statusMessage = null,
    diffPanel = null,
    diffOutput = null,
    diffEmpty = null,
    dirtyBadge = null,
    applyBtn = null,
    rollbackBtn = null
  } = elements;

  function setStatus(state, label, message) {
    if (!statusEl || !statusLabel || !statusMessage) return;
    statusEl.dataset.state = state;
    statusLabel.textContent = label;
    statusMessage.textContent = message;
  }

  function markDirty(isDirty) {
    console.debug('[UI] markDirty', isDirty);
    if (dirtyBadge) dirtyBadge.toggleAttribute('hidden', !isDirty);
    if (applyBtn) applyBtn.disabled = !isDirty;
    if (rollbackBtn) rollbackBtn.disabled = !isDirty;
    onDirtyChanged(Boolean(isDirty));
  }

  function updateDiff(isDirty) {
    if (!diffPanel || !diffOutput) return;
    const changes = runtime.diff();
    if (!isDirty || !changes.length) {
      diffPanel.setAttribute('hidden', '');
      diffOutput.textContent = '';
      diffEmpty?.removeAttribute('hidden');
      return;
    }
    diffPanel.removeAttribute('hidden');
    diffEmpty?.setAttribute('hidden', '');
    const maxVisibleChanges = 40;
    const visibleChanges = changes.slice(0, maxVisibleChanges);
    const lines = visibleChanges.map(({ path, before, after }) => {
      const beforeText = summarizeDiffValue(before);
      const afterText = summarizeDiffValue(after);
      return `• ${path}\n  before: ${beforeText}\n  after:  ${afterText}`;
    });
    if (changes.length > maxVisibleChanges) {
      lines.push(`… ${changes.length - maxVisibleChanges} additional change(s) hidden.`);
    }
    const title = `${changes.length} staged change${changes.length === 1 ? '' : 's'}`;
    diffOutput.textContent = `${title}\n\n${lines.join('\n\n')}`;
  }

  function showValidationErrors(errors = []) {
    diffPanel?.removeAttribute('hidden');
    if (!diffOutput) return;
    diffOutput.textContent = `Schema violations:\n${errors
      .map((e) => `• ${e.instancePath || '/'} ${e.message}`)
      .join('\n')}`;
  }

  function clearApplied(checksum = '') {
    diffPanel?.setAttribute('hidden', '');
    if (diffOutput) diffOutput.textContent = '';
    setStatus('ok', 'Device synced', `Checksum ${String(checksum).slice(0, 8)}…`);
  }

  return {
    setStatus,
    markDirty,
    updateDiff,
    showValidationErrors,
    clearApplied
  };
}
