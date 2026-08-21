import { test, expect } from '@playwright/test';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';
import {
  CONFIG_IDENTITY_VERSION,
  canonicalConfig,
  canonicalConfigJson,
  configDigest,
  equivalentConfig
} from '../runtime/config_identity.js';

const vectorsPath = fileURLToPath(
  new URL('../../tools/config_identity_vectors.json', import.meta.url)
);
const vectors = JSON.parse(fs.readFileSync(vectorsPath, 'utf8'));

test('canonical config identity matches the shared versioned vectors', async () => {
  expect(CONFIG_IDENTITY_VERSION).toBe(vectors.version);
  for (const vector of vectors.vectors) {
    expect(canonicalConfigJson(vector.left), vector.name).toBe(vector.canonical_json);
    expect(canonicalConfigJson(vector.right), vector.name).toBe(vector.canonical_json);
    expect(canonicalConfig(vector.left), vector.name).toEqual(canonicalConfig(vector.right));
    expect(equivalentConfig(vector.left, vector.right), vector.name).toBe(true);
    await expect(configDigest(vector.left), vector.name).resolves.toBe(vector.sha256);
    await expect(configDigest(vector.right), vector.name).resolves.toBe(vector.sha256);
  }
});

test('canonical config identity preserves array order and detects semantic changes', async () => {
  const first = { slots: [{ index: 0 }, { index: 1 }] };
  const reordered = { slots: [{ index: 1 }, { index: 0 }] };

  expect(equivalentConfig(first, reordered)).toBe(false);
  await expect(configDigest(first)).resolves.not.toBe(await configDigest(reordered));
});
