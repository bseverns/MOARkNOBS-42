const { strict: assert } = require('node:assert');

const {
  activeAlerts,
  describeAuthority,
  describeConfigValidation,
  describeDraft,
  formatTelemetryFreshness,
} = require('../ui/operator_state');

function run() {
  const now = Date.parse('2026-08-09T18:00:12Z');
  assert.deepEqual(
    formatTelemetryFreshness(
      {
        running: true,
        serialConnected: true,
        lastTelemetryAt: '2026-08-09T18:00:10Z',
      },
      now,
    ),
    { label: 'live', status: 'ok' },
  );
  assert.deepEqual(
    formatTelemetryFreshness(
      {
        running: true,
        serialConnected: true,
        lastTelemetryAt: '2026-08-09T18:00:00Z',
      },
      now,
    ),
    { label: 'stale · 12s', status: 'error' },
  );
  assert.deepEqual(
    describeConfigValidation({
      status: 'invalid',
      errors: [{ message: 'first' }, { message: 'second' }],
    }),
    { label: 'invalid · 2 errors', status: 'error', recoveryRequired: true },
  );
  assert.deepEqual(describeAuthority({ deviceAuthority: 'verified' }), {
    label: 'verified device truth',
    status: 'ok',
    recoveryRequired: false,
  });
  assert.deepEqual(describeDraft({ dirty: true, deviceAuthority: 'verified' }), {
    label: 'draft staged',
    status: 'warn',
    recoveryRequired: true,
  });
  assert.deepEqual(
    describeDraft({ deviceAuthority: 'verified-device-different' }),
    {
      label: 'device differs from draft',
      status: 'error',
      recoveryRequired: true,
    },
  );

  const active = [{ code: 'still_active' }];
  assert.equal(
    activeAlerts({ alerts: { active, recent: [{ code: 'already_cleared' }] } }),
    active,
    'Stage must render active alerts rather than alert history',
  );

  console.log('Bridge operator state distinguishes live truth, recovery, and alert history');
}

run();
