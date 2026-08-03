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

function splitDiffPath(path) {
  return String(path ?? '')
    .replace(/\[(\d+)\]/g, '.$1')
    .split('.')
    .filter(Boolean);
}

function humanizePathPart(value) {
  return String(value ?? '')
    .replace(/_/g, ' ')
    .replace(/([a-z0-9])([A-Z])/g, '$1 $2')
    .replace(/^./, (letter) => letter.toUpperCase());
}

function describeChangeGroup(path) {
  const parts = splitDiffPath(path);
  if (parts[0] === 'slots' && /^\d+$/.test(parts[1] ?? '')) {
    return { key: `slot-${parts[1]}`, label: `Slot ${Number(parts[1]) + 1}` };
  }
  const first = parts[0] || 'configuration';
  return { key: first, label: humanizePathPart(first) };
}

function describeChangePath(path) {
  const parts = splitDiffPath(path);
  const visible = parts[0] === 'slots' ? parts.slice(2) : parts.slice(1);
  return visible.length ? visible.map(humanizePathPart).join(' › ') : humanizePathPart(parts[0]);
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
    rollbackBtn = null,
    changeBar = null,
    changeCount = null,
    changeDiscardBtn = null,
    docRoot = null
  } = elements;

  let validationErrors = [];
  let transactionState = 'clean';
  let dirtyStartedAt = 0;

  function setStatus(state, label, message) {
    if (!statusEl || !statusLabel || !statusMessage) return;
    statusEl.dataset.state = state;
    statusLabel.textContent = label;
    statusMessage.textContent = message;
  }

  function markDirty(isDirty) {
    console.debug('[UI] markDirty', isDirty);
    if (dirtyBadge) dirtyBadge.toggleAttribute('hidden', !isDirty);
    if (changeBar) changeBar.toggleAttribute('hidden', !isDirty);
    if (docRoot) docRoot.dataset.dirty = isDirty ? 'true' : 'false';
    if (isDirty && !dirtyStartedAt) dirtyStartedAt = Date.now();
    if (!isDirty) dirtyStartedAt = 0;
    const applyAllowed = ['verified', 'simulator'].includes(
      runtime?.getState?.()?.contractQuality
    );
    const state = runtime?.getState?.() ?? {};
    // Older runtime snapshots only expose the compatibility transactionState.
    // Newer snapshots keep device authority separate from local draft dirtiness.
    const transactionWritable = state.deviceAuthority
      ? ['verified', 'verified-device-different'].includes(state.deviceAuthority)
      : ['dirty', 'verified', 'verified-device-different', 'clean'].includes(state.transactionState);
    const transactionBusy = ['preflighting', 'applying', 'uncertain', 'resynchronizing'].includes(
      transactionState
    );
    if (applyBtn) {
      applyBtn.disabled = !isDirty || !applyAllowed || !transactionWritable || transactionBusy;
    }
    if (rollbackBtn) rollbackBtn.disabled = !isDirty || !transactionWritable;
    if (changeDiscardBtn) changeDiscardBtn.disabled = !isDirty || !transactionWritable;
    onDirtyChanged(Boolean(isDirty));
  }

  function updateDiff(isDirty) {
    if (!diffPanel || !diffOutput) return;
    const changes = runtime.diff();
    if (changeCount) {
      changeCount.textContent = `${changes.length} staged change${changes.length === 1 ? '' : 's'}`;
    }
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
    validationErrors = Array.isArray(errors) ? errors : [];
    diffPanel?.removeAttribute('hidden');
    if (!diffOutput) return;
    diffOutput.textContent = `Schema violations:\n${errors
      .map((e) => `• ${e.instancePath || '/'} ${e.message}`)
      .join('\n')}`;
  }

  function clearApplied(checksum = '') {
    validationErrors = [];
    diffPanel?.setAttribute('hidden', '');
    if (diffOutput) diffOutput.textContent = '';
    setStatus('ok', 'Device synced', `Checksum ${String(checksum).slice(0, 8)}…`);
  }

  function setTransactionState(nextState = 'clean') {
    transactionState = nextState;
    if (!changeBar) return;
    changeBar.dataset.transactionState = nextState;
    const labels = {
      preflighting: 'Checking staged changes…',
      applying: 'Applying…',
      uncertain: 'Outcome uncertain',
      resynchronizing: 'Resynchronizing…',
      'verified-device-different': 'Device differs'
    };
    if (applyBtn) {
      applyBtn.textContent = labels[nextState] ?? 'Apply staged changes';
    }
    markDirty(Boolean(runtime?.getState?.()?.dirty));
  }

  function renderReview(container, { onNavigate = () => {} } = {}) {
    if (!container) return;
    container.innerHTML = '';
    if (validationErrors.length) {
      const validation = document.createElement('section');
      validation.className = 'review-validation-errors';
      const title = document.createElement('h3');
      title.textContent = `${validationErrors.length} validation error${validationErrors.length === 1 ? '' : 's'}`;
      const list = document.createElement('ul');
      validationErrors.forEach((error) => {
        const item = document.createElement('li');
        item.textContent = `${error.instancePath || '/'} ${error.message || 'is invalid'}`;
        list.append(item);
      });
      validation.append(title, list);
      container.append(validation);
    }

    const changes = runtime?.diff?.() ?? [];
    if (!changes.length) {
      const empty = document.createElement('p');
      empty.textContent = 'No staged changes.';
      container.append(empty);
      return;
    }

    const summary = document.createElement('p');
    summary.className = 'change-review-summary';
    summary.textContent = `${changes.length} staged change${changes.length === 1 ? '' : 's'} grouped by target.`;
    container.append(summary);

    const groups = new Map();
    changes.forEach((change) => {
      const descriptor = describeChangeGroup(change.path);
      if (!groups.has(descriptor.key)) groups.set(descriptor.key, { ...descriptor, changes: [] });
      groups.get(descriptor.key).changes.push(change);
    });

    groups.forEach((group) => {
      const section = document.createElement('section');
      section.className = 'change-review-group';
      const heading = document.createElement('h3');
      heading.textContent = `${group.label} · ${group.changes.length}`;
      section.append(heading);
      group.changes.forEach(({ path, before, after }) => {
        const row = document.createElement('article');
        row.className = 'change-review-row';
        const label = document.createElement('strong');
        label.textContent = describeChangePath(path);
        const values = document.createElement('div');
        values.className = 'change-review-values';
        const live = document.createElement('span');
        const liveBadge = document.createElement('b');
        liveBadge.textContent = 'Live';
        live.append(liveBadge, ` ${summarizeDiffValue(before)}`);
        const staged = document.createElement('span');
        const stagedBadge = document.createElement('b');
        stagedBadge.textContent = 'Staged';
        staged.append(stagedBadge, ` ${summarizeDiffValue(after)}`);
        values.append(live, staged);
        const focus = document.createElement('button');
        focus.type = 'button';
        focus.className = 'action-quiet';
        focus.textContent = 'Focus field';
        focus.addEventListener('click', () => onNavigate(path));
        row.append(label, values, focus);
        section.append(row);
      });
      container.append(section);
    });
  }

  function shouldConfirmDiscard() {
    const changeCountNow = runtime?.diff?.()?.length ?? 0;
    const longSession = dirtyStartedAt > 0 && Date.now() - dirtyStartedAt >= 5 * 60 * 1000;
    return changeCountNow >= 3 || longSession || docRoot?.dataset?.importedDraft === 'true';
  }

  return {
    setStatus,
    markDirty,
    updateDiff,
    showValidationErrors,
    clearApplied,
    setTransactionState,
    renderReview,
    shouldConfirmDiscard
  };
}
