const DEFAULT_THROTTLE_DELAY_MS = 24;

// Small event bus shared by the runtime and the view layer.
export function makeEmitter() {
  const listeners = new Map();
  return {
    on(event, handler) {
      const set = listeners.get(event) ?? new Set();
      set.add(handler);
      listeners.set(event, set);
      return () => set.delete(handler);
    },
    emit(event, payload) {
      const set = listeners.get(event);
      if (!set) return;
      for (const handler of [...set]) {
        try {
          handler(payload);
        } catch (err) {
          console.error('runtime listener error', event, err);
        }
      }
    }
  };
}

// Avoid repeated constructor churn when we encode serial/WebSocket payloads.
export function encoder() {
  return new TextEncoder();
}

// Keep a matching decoder helper beside the encoder for transport adapters.
export function decoder() {
  return new TextDecoder();
}

// Hash staged config payloads so Apply can demand an explicit firmware ACK.
export async function digest(message) {
  const bytes = typeof message === 'string' ? new TextEncoder().encode(message) : message;
  const hash = await crypto.subtle.digest('SHA-256', bytes);
  const view = new DataView(hash);
  let hex = '';
  for (let i = 0; i < view.byteLength; i += 4) {
    const value = view.getUint32(i);
    hex += value.toString(16).padStart(8, '0');
  }
  return hex;
}

// Structured clone when available; JSON clone fallback keeps tests/browser support simple.
export function clone(value) {
  if (value === undefined) return value;
  if (typeof structuredClone === 'function') return structuredClone(value);
  return JSON.parse(JSON.stringify(value));
}

// Diff staged vs live state for the dirty badge and human-readable change panel.
export function shallowDiff(before, after, basePath = '') {
  const changes = [];
  if (before === after) return changes;
  if (typeof before !== 'object' || typeof after !== 'object' || !before || !after) {
    changes.push({ path: basePath || '.', before, after });
    return changes;
  }
  const keys = new Set([...Object.keys(before), ...Object.keys(after)]);
  for (const key of keys) {
    const nextPath = basePath ? `${basePath}.${key}` : key;
    const prev = before[key];
    const next = after[key];
    if (typeof prev === 'object' && typeof next === 'object') {
      changes.push(...shallowDiff(prev ?? null, next ?? null, nextPath));
    } else if (prev !== next) {
      changes.push({ path: nextPath, before: prev, after: next });
    }
  }
  return changes;
}

// Cheap field-level equality check for tiny metadata records.
export function shallowEqual(a, b) {
  if (a === b) return true;
  if (typeof a !== 'object' || typeof b !== 'object' || !a || !b) return false;
  const keys = new Set([...Object.keys(a), ...Object.keys(b)]);
  for (const key of keys) {
    if (a[key] !== b[key]) return false;
  }
  return true;
}

// Convert dotted paths like `slots.3.data1` into array/object access tokens.
function parseSegment(segment) {
  const idx = Number(segment);
  return Number.isInteger(idx) && String(idx) === segment ? idx : segment;
}

// Mutate a nested draft object in place for staged edits and test harness helpers.
export function setNestedValue(target, path, value) {
  if (!path) return;
  const segments = path.split('.').map(parseSegment);
  let cursor = target;
  for (let i = 0; i < segments.length; i += 1) {
    const segment = segments[i];
    if (i === segments.length - 1) {
      cursor[segment] = value;
      break;
    }
    const nextSegment = segments[i + 1];
    if (cursor[segment] === undefined || cursor[segment] === null) {
      cursor[segment] = typeof nextSegment === 'number' ? [] : {};
    }
    cursor = cursor[segment];
  }
}

// Chunk large native `SET_ALL` bodies so serial transport stays within line limits.
export function chunkString(value, size) {
  const text = typeof value === 'string' ? value : String(value ?? '');
  const chunkSize = Math.max(1, Math.floor(Number(size) || 1));
  const chunks = [];
  for (let index = 0; index < text.length; ) {
    let end = Math.min(text.length, index + chunkSize);
    if (end < text.length && text[end] === '{') {
      end += 1;
    }
    chunks.push(text.slice(index, end));
    index = end;
  }
  return chunks.length ? chunks : [''];
}

// UI throttling helper for controls that should feel live without flooding transport.
export function createThrottle(fn, delay = DEFAULT_THROTTLE_DELAY_MS) {
  let timer = null;
  let pendingArgs = null;
  let lastRun = 0;
  return (...args) => {
    pendingArgs = args;
    const now = performance.now();
    const invoke = () => {
      lastRun = performance.now();
      timer = null;
      const runArgs = pendingArgs;
      pendingArgs = null;
      fn(...runArgs);
    };
    if (!timer) {
      const since = now - lastRun;
      const wait = since >= delay ? 0 : delay - since;
      timer = setTimeout(invoke, wait);
    }
  };
}
