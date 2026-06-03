import { test, expect } from '@playwright/test';
import {
  chunkString,
  clone,
  createThrottle,
  digest,
  makeEmitter,
  setNestedValue,
  shallowDiff,
  shallowEqual
} from '../runtime/runtime_utils.js';

test('runtime utility event emitter notifies and unsubscribes listeners', () => {
  const events = [];
  const { emit, on } = makeEmitter();
  const unsubscribe = on('status', (payload) => events.push(payload));

  emit('status', { ready: true });
  unsubscribe();
  emit('status', { ready: false });

  expect(events).toEqual([{ ready: true }]);
});

test('runtime utility clone creates an independent nested copy', () => {
  const source = { slots: [{ value: 12 }] };
  const copied = clone(source);

  copied.slots[0].value = 40;

  expect(source.slots[0].value).toBe(12);
  expect(copied.slots[0].value).toBe(40);
});

test('runtime utility shallowDiff reports nested staged/live path changes', () => {
  const before = { slots: [{ value: 12, cc: 74 }] };
  const after = { slots: [{ value: 24, cc: 74 }], mode: 'stage' };

  expect(shallowDiff(before, after)).toEqual([
    { path: 'slots.0.value', before: 12, after: 24 },
    { path: 'mode', before: undefined, after: 'stage' }
  ]);
});

test('runtime utility shallowEqual compares small metadata records', () => {
  expect(
    shallowEqual({ label: 'Lead', takeControl: true }, { label: 'Lead', takeControl: true })
  ).toBe(true);
  expect(shallowEqual({ label: 'Lead' }, { label: 'Bass' })).toBe(false);
  expect(shallowEqual({ label: 'Lead' }, null)).toBe(false);
});

test('runtime utility setNestedValue creates array and object containers', () => {
  const target = {};

  setNestedValue(target, 'slots.0.value', 64);
  setNestedValue(target, 'slots.0.route.mode', 'add');

  expect(target).toEqual({ slots: [{ value: 64, route: { mode: 'add' } }] });
});

test('runtime utility chunkString preserves brace boundaries and handles empty input', () => {
  expect(chunkString('abc{def', 3)).toEqual(['abc{', 'def']);
  expect(chunkString('', 10)).toEqual(['']);
  expect(chunkString('abcdef', 2)).toEqual(['ab', 'cd', 'ef']);
});

test('runtime utility digest returns stable SHA-256 hex', async () => {
  await expect(digest('hello')).resolves.toBe(
    '2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824'
  );
});

test('runtime utility createThrottle coalesces calls inside the delay window', async () => {
  const calls = [];
  const throttled = createThrottle((value) => calls.push(value), 20);

  throttled(1);
  throttled(2);
  await new Promise((resolve) => setTimeout(resolve, 50));

  expect(calls).toEqual([2]);
});
