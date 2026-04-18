import { test, expect } from '@playwright/test';
import fs from 'node:fs';
import path from 'node:path';

const SUPPORTED = new Set([
  'type',
  'enum',
  'minimum',
  'maximum',
  'maxLength',
  'pattern',
  'required',
  'additionalProperties',
  'properties',
  'anyOf',
  'minItems',
  'maxItems',
  'items',
  'uniqueItems'
]);

const NON_VALIDATION = new Set(['$schema', 'title', 'description', 'default', 'schema_version']);

function walkSchema(node, cursor, used, unknown) {
  if (!node || typeof node !== 'object' || Array.isArray(node)) return;

  for (const [key, value] of Object.entries(node)) {
    if (SUPPORTED.has(key)) {
      used.add(key);
    } else if (!NON_VALIDATION.has(key)) {
      unknown.push(`${cursor} -> ${key}`);
    }

    if (key === 'properties' && value && typeof value === 'object' && !Array.isArray(value)) {
      for (const [propName, propSchema] of Object.entries(value)) {
        walkSchema(propSchema, `${cursor}/properties/${propName}`, used, unknown);
      }
      continue;
    }

    if (key === 'items') {
      walkSchema(value, `${cursor}/items`, used, unknown);
      continue;
    }

    if (key === 'anyOf' && Array.isArray(value)) {
      value.forEach((candidate, index) =>
        walkSchema(candidate, `${cursor}/anyOf/${index}`, used, unknown)
      );
    }
  }
}

test('schema keywords stay within mini-ajv support boundary', async () => {
  const schemaPath = path.resolve(process.cwd(), 'config_schema.json');
  const schema = JSON.parse(fs.readFileSync(schemaPath, 'utf8'));

  const used = new Set();
  const unknown = [];
  walkSchema(schema, '#', used, unknown);

  const unsupported = [...used].filter((keyword) => !SUPPORTED.has(keyword)).sort();

  expect(unsupported).toEqual([]);
  expect(unknown).toEqual([]);
});
