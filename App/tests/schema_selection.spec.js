import { test, expect } from '@playwright/test';
import fs from 'node:fs';
import path from 'node:path';
import {
  findUnsupportedSchemaKeywords,
  selectSchemaForHydration
} from '../runtime/schema_selection.js';

const compatibleSchema = {
  type: 'object',
  required: ['slots', 'efSlots', 'filter', 'arg', 'led'],
  properties: {
    slots: { type: 'array' },
    efSlots: { type: 'array' },
    filter: { type: 'object' },
    arg: { type: 'object' },
    led: { type: 'object' }
  }
};

test('unsupported device-schema constraints are rejected before MiniAjv compilation', async () => {
  const deviceSchema = {
    ...compatibleSchema,
    properties: {
      ...compatibleSchema.properties,
      filter: { type: 'object', oneOf: [{ type: 'object' }] }
    }
  };
  expect(findUnsupportedSchemaKeywords(deviceSchema)).toContain('#/properties/filter/oneOf');

  const selection = await selectSchemaForHydration({
    sendRpc: async () => ({ schema: deviceSchema }),
    schemaUrl: 'unused',
    fetchJson: async () => compatibleSchema
  });
  expect(selection).toMatchObject({ source: 'bundled', quality: 'incompatible' });
});

test('failed device-schema retrieval is visibly classified as fallback schema', async () => {
  const status = [];
  const selection = await selectSchemaForHydration({
    sendRpc: async () => {
      throw new Error('schema unavailable');
    },
    schemaUrl: 'unused',
    fetchJson: async () => compatibleSchema,
    emit: (type, payload) => status.push({ type, payload })
  });
  expect(selection).toMatchObject({ source: 'bundled', quality: 'fallback-schema' });
  expect(status).toContainEqual(
    expect.objectContaining({ type: 'status', payload: expect.objectContaining({ level: 'warn' }) })
  );
});

test('the bundled firmware schema metadata is accepted by the device-schema gate', async () => {
  const bundled = JSON.parse(
    fs.readFileSync(path.resolve(process.cwd(), 'config_schema.json'), 'utf8')
  );
  expect(bundled.schema_version).toBeDefined();
  expect(findUnsupportedSchemaKeywords(bundled)).toEqual([]);

  const selection = await selectSchemaForHydration({
    sendRpc: async () => ({ schema: bundled }),
    schemaUrl: 'unused',
    fetchJson: async () => bundled
  });
  expect(selection).toMatchObject({ source: 'device', quality: 'verified' });
});

test('firmware x_mn42 authority metadata does not downgrade a compatible device schema', async () => {
  const deviceSchema = {
    ...compatibleSchema,
    schema_version: 8,
    x_mn42: {
      authority: 'device',
      configurator: 'convenience editor for staged user input',
      bridge: 'required when USB/WebSerial transport is unavailable'
    }
  };

  expect(findUnsupportedSchemaKeywords(deviceSchema)).toEqual([]);
  const selection = await selectSchemaForHydration({
    sendRpc: async () => ({ schema: deviceSchema }),
    schemaUrl: 'unused',
    fetchJson: async () => compatibleSchema
  });
  expect(selection).toMatchObject({ source: 'device', quality: 'verified' });
});
