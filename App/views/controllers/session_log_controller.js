const SESSION_LOG_LIMIT = 500;

function pad(value) {
  return String(value).padStart(2, '0');
}

function formatTimestamp(value) {
  const date = value instanceof Date ? value : new Date(value);
  if (Number.isNaN(date.getTime())) return '0000-00-00 00:00:00';
  return [date.getFullYear(), pad(date.getMonth() + 1), pad(date.getDate())]
    .join('-')
    .concat(` ${pad(date.getHours())}:${pad(date.getMinutes())}:${pad(date.getSeconds())}`);
}

function normalizeEntry(entry = {}) {
  return {
    timestamp:
      typeof entry.timestamp === 'string' && entry.timestamp.trim()
        ? entry.timestamp
        : new Date().toISOString(),
    level:
      typeof entry.level === 'string' && entry.level.trim() ? entry.level.toUpperCase() : 'INFO',
    category:
      typeof entry.category === 'string' && entry.category.trim()
        ? entry.category.toUpperCase()
        : 'NOTE',
    message: typeof entry.message === 'string' ? entry.message : '',
    detail: typeof entry.detail === 'string' ? entry.detail : ''
  };
}

function toDisplayLine(entry) {
  const parts = [
    `[${formatTimestamp(entry.timestamp)}]`,
    entry.level.padEnd(4, ' '),
    entry.category
  ];
  if (entry.message) parts.push(entry.message);
  if (entry.detail) parts.push(`:: ${entry.detail}`);
  return parts.join(' ');
}

export function createSessionLogController({
  logEl,
  countEl,
  storage,
  storageKey = 'moarknobs:session-log',
  exportBtn,
  clearBtn,
  now = () => new Date()
} = {}) {
  let entries = [];

  function persist() {
    if (!storage) return;
    try {
      if (!entries.length) {
        storage.removeItem(storageKey);
        return;
      }
      storage.setItem(storageKey, JSON.stringify(entries));
    } catch (err) {
      console.debug('persist session log failed', err);
    }
  }

  function render() {
    if (logEl) {
      logEl.textContent = entries.length
        ? entries.map((entry) => toDisplayLine(entry)).join('\n')
        : 'No session events yet.';
      logEl.scrollTop = logEl.scrollHeight;
    }
    if (countEl) {
      countEl.textContent = `${entries.length} ${
        entries.length === 1 ? 'entry' : 'entries'
      } stored in this browser`;
    }
    if (exportBtn) exportBtn.disabled = entries.length === 0;
    if (clearBtn) clearBtn.disabled = entries.length === 0;
  }

  function load() {
    if (!storage) {
      render();
      return;
    }
    try {
      const raw = storage.getItem(storageKey);
      const parsed = raw ? JSON.parse(raw) : [];
      entries = Array.isArray(parsed)
        ? parsed.map((entry) => normalizeEntry(entry)).slice(-SESSION_LOG_LIMIT)
        : [];
    } catch (err) {
      console.debug('read session log failed', err);
      entries = [];
    }
    render();
  }

  function add(entry) {
    entries = [...entries, normalizeEntry(entry)].slice(-SESSION_LOG_LIMIT);
    persist();
    render();
  }

  function recordStatus(state, label, message) {
    add({
      timestamp: now().toISOString(),
      level: typeof state === 'string' ? state : 'info',
      category: typeof label === 'string' ? label : 'STATUS',
      message: typeof message === 'string' ? message : ''
    });
  }

  function recordEvent(category, message, detail = '', level = 'info') {
    add({
      timestamp: now().toISOString(),
      level,
      category,
      message,
      detail
    });
  }

  function clear() {
    entries = [];
    persist();
    render();
  }

  function exportLog() {
    if (!entries.length) return null;
    const timestamp = now().toISOString();
    const filename = `moarknobz-session-log-${timestamp.replace(/[:.]/g, '-')}.txt`;
    const body = [
      'MOARkNOBS-42 Session Log',
      `Generated: ${timestamp}`,
      `Entries: ${entries.length}`,
      '',
      ...entries.map((entry) => toDisplayLine(entry))
    ].join('\n');
    const blob = new Blob([body], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = filename;
    anchor.style.display = 'none';
    document.body.appendChild(anchor);
    anchor.click();
    setTimeout(() => {
      URL.revokeObjectURL(url);
      anchor.remove();
    }, 0);
    return filename;
  }

  function bind() {
    exportBtn?.addEventListener('click', () => {
      const filename = exportLog();
      if (filename) {
        recordEvent('SESSION', 'Log exported', filename, 'ok');
      }
    });
    clearBtn?.addEventListener('click', () => {
      clear();
    });
  }

  load();

  return {
    bind,
    clear,
    exportLog,
    load,
    recordEvent,
    recordStatus
  };
}
