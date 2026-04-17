import { test, expect } from '@playwright/test';
import MiniAjv from '../lib/mini-ajv.js';

test('mini-ajv enforces anyOf and uniqueItems used by config schema', async () => {
  const ajv = new MiniAjv({ allErrors: true });
  const validator = ajv.compile({
    type: 'object',
    properties: {
      slot: { type: 'integer' },
      slots: {
        type: 'array',
        items: { type: 'integer' },
        uniqueItems: true
      }
    },
    anyOf: [{ required: ['slot'] }, { required: ['slots'] }],
    additionalProperties: false
  });

  const missingBranch = validator({});
  const missingBranchErrors = (validator.errors ?? []).map((entry) => entry.keyword);

  const duplicateSlots = validator({ slots: [4, 4] });
  const duplicateSlotsErrors = (validator.errors ?? []).map((entry) => entry.keyword);

  const valid = validator({ slots: [4, 9] });
  const result = {
    missingBranch,
    missingBranchErrors,
    duplicateSlots,
    duplicateSlotsErrors,
    valid
  };

  expect(result.missingBranch).toBe(false);
  expect(result.missingBranchErrors).toContain('anyOf');

  expect(result.duplicateSlots).toBe(false);
  expect(result.duplicateSlotsErrors).toContain('uniqueItems');

  expect(result.valid).toBe(true);
});
