const { strict: assert } = require('node:assert');

const {
  activeAlerts,
  changedSlotIndices,
  describeAuthority,
  describeConfigValidation,
  describeDraft,
  describeRouteHeartbeats,
  formatTelemetryFreshness,
  isActionVisibleInMode,
  latestLearnableMidiCc,
  observedSoundcheckLanes,
  operatorConfirmationMessage,
  parseSlotTelemetryLine,
  recentOscAddresses,
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

  const routes = [
    {
      flow: 'serial->osc',
      traceId: 'movement-1',
      hostTimestampMs: now - 1000,
    },
    {
      flow: 'serial->midi',
      traceId: 'movement-1',
      hostTimestampMs: now - 800,
    },
    { flow: 'osc->serial', hostTimestampMs: now - 12_000 },
  ];
  assert.deepEqual(describeRouteHeartbeats(routes, now), {
    deviceOsc: { label: 'active now', status: 'ok', recent: true },
    deviceMidi: { label: 'active now', status: 'ok', recent: true },
    oscDevice: { label: 'seen 12s ago', status: 'muted', recent: false },
    midiDevice: { label: 'not seen', status: 'muted', recent: false },
  });
  assert.deepEqual(
    parseSlotTelemetryLine(
      JSON.stringify({ type: 'telemetry', traceId: 'movement-1', slots: [10, 64] }),
    ),
    { traceId: 'movement-1', slots: [10, 64] },
  );
  assert.deepEqual(changedSlotIndices([10, 64], [10, 65]), [1]);
  assert.deepEqual(
    [...observedSoundcheckLanes(routes, { traceId: 'movement-1' })].sort(),
    ['deviceMidi', 'deviceOsc'],
  );
  assert.equal(isActionVisibleInMode('setup mappings advanced', 'mappings'), true);
  assert.equal(isActionVisibleInMode('setup mappings advanced', 'stage'), false);
  assert.match(operatorConfirmationMessage('stop'), /routing.*disconnect/i);
  assert.match(operatorConfirmationMessage('clearAlerts'), /unresolved conditions.*raise again/i);
  assert.deepEqual(
    latestLearnableMidiCc(
      [
        {
          flow: 'midi->serial',
          kind: 'drop_feedback',
          status: 0xb0,
          slot: 4,
          value: 60,
          hostTimestampMs: now - 100,
        },
        {
          flow: 'midi->serial',
          kind: 'command',
          status: 0xb2,
          slot: 21,
          value: 91,
          hostTimestampMs: now,
        },
      ],
      now - 1000,
    ),
    { channel: 3, controller: 21, value: 91, observedAt: now },
    'MIDI learn should capture intentional CC input and ignore feedback drops',
  );
  assert.equal(latestLearnableMidiCc(routes, now + 1), null);
  assert.deepEqual(
    recentOscAddresses([
      { flow: 'serial->osc', address: '/mn42/slots' },
      { flow: 'midi->osc', address: '/show/filter' },
      { flow: 'serial->osc', address: '/mn42/slots' },
      { flow: 'osc->serial', address: '/mn42/cmd' },
    ]),
    ['/show/filter'],
    'OSC suggestions should be recent, unique custom outbound destinations',
  );

  console.log('Bridge operator state covers truth, soundcheck, show safety, and mapping learn');
}

run();
