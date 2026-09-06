const { strict: assert } = require('node:assert');

const {
  activeAlerts,
  changedSlotIndices,
  describeAuthority,
  describeConfigValidation,
  describeDraft,
  describeRouteHeartbeats,
  describeRoutingDestinations,
  createHostSetupEnvelope,
  formatTelemetryFreshness,
  hostSetupConfigFingerprint,
  isActionVisibleInMode,
  latestLearnableMidiCc,
  normalizeHostSetupConfig,
  observedSoundcheckLanes,
  expectedSoundcheckLanes,
  parseOutboundRouteDraft,
  operatorConfirmationMessage,
  parseSlotTelemetryLine,
  parseHostSetupEnvelope,
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
  assert.deepEqual(
    describeDraft({ dirty: true, deviceAuthority: 'verified' }),
    {
      label: 'draft staged',
      status: 'warn',
      recoveryRequired: true,
    },
  );
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
    describeRoutingDestinations({
      oscDestinationName: 'TouchDesigner',
      oscHost: '192.168.1.20',
      oscPort: 9000,
      midiDestinationName: 'Ableton',
      midiLabel: 'MN42 IAC',
    }),
    {
      deviceOsc: 'TouchDesigner · OSC 192.168.1.20:9000',
      deviceMidi: 'Ableton · MIDI MN42 IAC',
      oscDevice: 'TouchDesigner · OSC input',
      midiDevice: 'Ableton · MIDI input',
    },
  );
  assert.deepEqual(
    parseSlotTelemetryLine(
      JSON.stringify({
        type: 'telemetry',
        traceId: 'movement-1',
        slots: [10, 64],
      }),
    ),
    { traceId: 'movement-1', slots: [10, 64] },
  );
  assert.deepEqual(changedSlotIndices([10, 64], [10, 65]), [1]);
  assert.deepEqual(
    [...observedSoundcheckLanes(routes, { traceId: 'movement-1' })].sort(),
    ['deviceMidi', 'deviceOsc'],
  );
  assert.deepEqual(expectedSoundcheckLanes({}, [11]), [
    'deviceOsc',
    'deviceMidi',
  ]);
  const mappedConfig = {
    midiTelemetryMode: 'mapped',
    outboundMidiMappings: [
      { source: 'slots', sourceIndex: 11 },
      { source: 'envelopes', sourceIndex: 0 },
    ],
  };
  assert.deepEqual(expectedSoundcheckLanes(mappedConfig, [11]), [
    'deviceOsc',
    'deviceMidi',
  ]);
  assert.deepEqual(expectedSoundcheckLanes(mappedConfig, [0]), ['deviceOsc']);
  assert.deepEqual(
    expectedSoundcheckLanes({ midiTelemetryMode: 'mapped' }, [11]),
    ['deviceOsc'],
  );
  assert.deepEqual(
    [
      ...observedSoundcheckLanes(
        [
          {
            flow: 'serial->midi',
            kind: 'drop_feedback',
            traceId: 'movement-1',
          },
          { flow: 'serial->osc', kind: 'event', traceId: 'movement-1' },
          { flow: 'serial->osc', kind: 'telemetry', traceId: 'other-movement' },
        ],
        { traceId: 'movement-1' },
      ),
    ],
    [],
    'Unrelated events and dropped routes cannot pass soundcheck',
  );
  assert.equal(
    describeRouteHeartbeats(
      [{ flow: 'osc->serial', kind: 'drop_feedback', hostTimestampMs: now }],
      now,
    ).oscDevice.recent,
    false,
    'Dropped traffic must not light a successful path',
  );
  assert.deepEqual(
    [
      ...observedSoundcheckLanes(
        [
          {
            flow: 'serial->osc',
            kind: 'telemetry',
            hostTimestampMs: now - 5000,
          },
        ],
        { detectedAt: now },
      ),
    ],
    [],
    'Old telemetry cannot pass a new soundcheck',
  );
  const routeDraft = [
    { source: 'slots', sourceIndex: 11, controller: 74, channel: 1 },
  ];
  assert.equal(
    parseOutboundRouteDraft(JSON.stringify(routeDraft))[0].sourceIndex,
    11,
  );
  assert.throws(() => parseOutboundRouteDraft('{}'), /array/);
  assert.throws(
    () =>
      parseOutboundRouteDraft(
        JSON.stringify([{ ...routeDraft[0], channel: 17 }]),
      ),
    /Route 1/,
  );
  assert.throws(
    () =>
      parseOutboundRouteDraft(
        JSON.stringify([{ ...routeDraft[0], sourceIndex: null }]),
      ),
    /Route 1/,
  );
  assert.throws(
    () =>
      parseOutboundRouteDraft(
        JSON.stringify([{ ...routeDraft[0], controller: '' }]),
      ),
    /Route 1/,
  );
  assert.throws(
    () =>
      parseOutboundRouteDraft(JSON.stringify(Array(129).fill(routeDraft[0]))),
    /128/,
  );
  assert.throws(
    () =>
      parseOutboundRouteDraft(
        JSON.stringify([
          { ...routeDraft[0], id: 'same' },
          { ...routeDraft[0], id: 'same' },
        ]),
      ),
    /unique/,
  );
  assert.equal(
    isActionVisibleInMode('setup mappings advanced', 'mappings'),
    true,
  );
  assert.equal(
    isActionVisibleInMode('setup mappings advanced', 'stage'),
    false,
  );
  assert.match(operatorConfirmationMessage('stop'), /routing.*disconnect/i);
  assert.match(
    operatorConfirmationMessage('clearAlerts'),
    /unresolved conditions.*raise again/i,
  );
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

  const hostConfig = {
    serialName: '/dev/cu.usbmodem123',
    midiLabel: 'Friday IAC',
    midiDestinationName: 'Ableton',
    oscHost: '127.0.0.1',
    oscPort: 9000,
    oscDestinationName: 'TouchDesigner',
    oscListen: 9001,
    oscBind: '127.0.0.1',
    feedbackWindowMs: 120,
    rtP95TargetMs: 10,
    rtJitterP95TargetMs: 5,
    alertSuppressionMs: 3000,
    allowFeedbackLoops: false,
    midiToOscMappings: [
      {
        id: '<b>filter</b>',
        controller: 21,
        channel: 3,
        address: '/show/filter',
        valueMode: 'normalized',
      },
      { controller: 999, address: '/invalid' },
    ],
  };
  const normalizedHostConfig = normalizeHostSetupConfig(hostConfig);
  assert.equal(normalizedHostConfig.midiToOscMappings.length, 1);
  assert.equal(normalizedHostConfig.midiToOscMappings[0].id, '<b>filter</b>');
  const envelope = createHostSetupEnvelope(
    [
      {
        id: 'friday-show',
        name: 'Friday show',
        lastUsedAt: '2026-08-10T18:00:00.000Z',
        notes: 'Launch visuals before routing.',
        suggestedDeviceProfile: 'Performance A',
        createdAt: '2026-08-09T18:00:00.000Z',
        updatedAt: '2026-08-09T18:00:00.000Z',
        config: hostConfig,
      },
      { id: 'broken', name: '', config: {} },
    ],
    '2026-08-09T18:00:00.000Z',
  );
  assert.equal(envelope.format, 'mn42-bridge-host-setups');
  assert.equal(envelope.version, 1);
  assert.equal(envelope.setups.length, 1);
  assert.equal(envelope.setups[0].suggestedDeviceProfile, 'Performance A');
  assert.equal(envelope.setups[0].lastUsedAt, '2026-08-10T18:00:00.000Z');
  assert.deepEqual(parseHostSetupEnvelope(envelope), {
    setups: envelope.setups,
    rejected: 0,
    valid: true,
  });
  assert.equal(parseHostSetupEnvelope({ version: 1 }).valid, false);
  assert.equal(
    hostSetupConfigFingerprint(hostConfig),
    hostSetupConfigFingerprint(normalizedHostConfig),
  );

  console.log(
    'Bridge operator state covers mapping learn and versioned Performance Setups',
  );
}

run();
