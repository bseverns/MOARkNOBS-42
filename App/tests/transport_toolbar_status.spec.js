import { test, expect } from '@playwright/test';
import {
  describeNotAppliedStatus
} from '../views/controllers/transport_toolbar_controller.js';

test('toolbar reports a dirty normalized-clean Apply as a no-op', () => {
  expect(
    describeNotAppliedStatus({ applied: false, reason: 'clean' }, true)
  ).toEqual({
    level: 'warn',
    label: 'No device write needed',
    message:
      'The Bridge considered the normalized device state unchanged; your local draft remains staged.'
  });
});

test('toolbar reports an already-synchronized normalized-clean Apply', () => {
  expect(
    describeNotAppliedStatus({ applied: false, reason: 'clean' }, false)
  ).toEqual({
    level: 'ok',
    label: 'No device write needed',
    message: 'The device configuration was already synchronized.'
  });
});
