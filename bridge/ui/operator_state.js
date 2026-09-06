(function attachOperatorState(root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.MN42BridgeOperatorState = api;
})(typeof globalThis === 'object' ? globalThis : this, () => {
  const ROUTE_LANES = Object.freeze({
    'serial->osc': 'deviceOsc',
    'serial->midi': 'deviceMidi',
    'osc->serial': 'oscDevice',
    'midi->serial': 'midiDevice',
  });
  const HOST_SETUP_FORMAT = 'mn42-bridge-host-setups';
  const HOST_SETUP_VERSION = 1;

  function shortString(value, maxLength = 256) {
    return typeof value === 'string' ? value.trim().slice(0, maxLength) : '';
  }

  function integerInRange(value, min, max, fallback = null) {
    const number = Number(value);
    return Number.isInteger(number) && number >= min && number <= max
      ? number
      : fallback;
  }

  function positiveNumber(value, fallback) {
    const number = Number(value);
    return Number.isFinite(number) && number > 0 ? number : fallback;
  }

  function normalizeHostMapping(mapping, index) {
    if (!mapping || typeof mapping !== 'object' || Array.isArray(mapping)) {
      return null;
    }
    const controller = integerInRange(mapping.controller ?? mapping.cc, 0, 127);
    const channel =
      mapping.channel === null || mapping.channel === ''
        ? null
        : integerInRange(mapping.channel, 1, 16);
    const address = shortString(mapping.address, 256);
    const invalidChannel =
      channel === null && mapping.channel != null && mapping.channel !== '';
    if (controller === null || invalidChannel || !address.startsWith('/')) {
      return null;
    }
    return {
      id: shortString(mapping.id, 80) || `mapping-${index + 1}`,
      kind: 'cc',
      controller,
      channel,
      address,
      valueMode: mapping.valueMode === 'normalized' ? 'normalized' : 'raw',
      scale: Number.isFinite(Number(mapping.scale)) ? Number(mapping.scale) : 1,
      offset: Number.isFinite(Number(mapping.offset))
        ? Number(mapping.offset)
        : 0,
      argType: mapping.argType === 'int' ? 'int' : 'float',
    };
  }

  function normalizeOutboundMidiMapping(mapping, index) {
    if (!mapping || typeof mapping !== 'object' || Array.isArray(mapping))
      return null;
    const source = shortString(mapping.source, 32).toLowerCase();
    const sourceIndex = integerInRange(
      mapping.sourceIndex ?? mapping.index,
      0,
      127,
    );
    const channel = integerInRange(mapping.channel, 1, 16);
    const controller = integerInRange(mapping.controller ?? mapping.cc, 0, 127);
    if (
      !['slots', 'envelopes'].includes(source) ||
      sourceIndex === null ||
      channel === null ||
      controller === null
    ) {
      return null;
    }
    return {
      id: shortString(mapping.id, 80) || `outbound-${index + 1}`,
      source,
      sourceIndex,
      channel,
      controller,
    };
  }

  function normalizeHostSetupConfig(config) {
    if (!config || typeof config !== 'object' || Array.isArray(config)) {
      return null;
    }
    const oscHost = shortString(config.oscHost, 256);
    const oscBind = shortString(config.oscBind, 256);
    const oscPort = integerInRange(config.oscPort, 1, 65535);
    const oscListen = integerInRange(config.oscListen, 1, 65535);
    if (!oscHost || !oscBind || oscPort === null || oscListen === null) {
      return null;
    }
    const rawMappings = Array.isArray(config.midiToOscMappings)
      ? config.midiToOscMappings.slice(0, 128)
      : [];
    const rawOutboundMappings = Array.isArray(config.outboundMidiMappings)
      ? config.outboundMidiMappings.slice(0, 128)
      : [];
    return {
      serialName: shortString(config.serialName, 256),
      midiLabel: shortString(config.midiLabel, 256),
      midiDestinationName:
        shortString(config.midiDestinationName, 80) || 'MIDI destination',
      oscHost,
      oscPort,
      oscDestinationName:
        shortString(config.oscDestinationName, 80) || 'OSC destination',
      oscListen,
      oscBind,
      feedbackWindowMs: positiveNumber(config.feedbackWindowMs, 120),
      rtP95TargetMs: positiveNumber(config.rtP95TargetMs, 10),
      rtJitterP95TargetMs: positiveNumber(config.rtJitterP95TargetMs, 5),
      alertSuppressionMs: positiveNumber(config.alertSuppressionMs, 3000),
      allowFeedbackLoops: Boolean(config.allowFeedbackLoops),
      midiToOscMappings: rawMappings.map(normalizeHostMapping).filter(Boolean),
      midiTelemetryMode:
        config.midiTelemetryMode === 'mapped' ? 'mapped' : 'legacy',
      outboundMidiMappings: rawOutboundMappings
        .map(normalizeOutboundMidiMapping)
        .filter(Boolean),
    };
  }

  function normalizeHostSetup(setup) {
    if (!setup || typeof setup !== 'object' || Array.isArray(setup))
      return null;
    const id = shortString(setup.id, 96);
    const name = shortString(setup.name, 80);
    const config = normalizeHostSetupConfig(setup.config);
    if (!id || !name || !config) return null;
    return {
      id,
      name,
      notes: shortString(setup.notes, 500) || null,
      suggestedDeviceProfile:
        shortString(setup.suggestedDeviceProfile, 80) || null,
      createdAt: shortString(setup.createdAt, 64) || null,
      updatedAt: shortString(setup.updatedAt, 64) || null,
      lastUsedAt: shortString(setup.lastUsedAt, 64) || null,
      config,
    };
  }

  function parseHostSetupEnvelope(value) {
    if (
      !value ||
      typeof value !== 'object' ||
      value.format !== HOST_SETUP_FORMAT ||
      value.version !== HOST_SETUP_VERSION ||
      !Array.isArray(value.setups)
    ) {
      return { setups: [], rejected: 0, valid: false };
    }
    const source = value.setups.slice(0, 50);
    const setups = source.map(normalizeHostSetup).filter(Boolean);
    return {
      setups,
      rejected: value.setups.length - setups.length,
      valid: true,
    };
  }

  function createHostSetupEnvelope(
    setups = [],
    exportedAt = new Date().toISOString(),
  ) {
    return {
      format: HOST_SETUP_FORMAT,
      version: HOST_SETUP_VERSION,
      exportedAt,
      setups: setups.slice(0, 50).map(normalizeHostSetup).filter(Boolean),
    };
  }

  function hostSetupConfigFingerprint(config) {
    const normalized = normalizeHostSetupConfig(config);
    return normalized ? JSON.stringify(normalized) : '';
  }

  function ageSeconds(isoString, nowMs) {
    const timestamp = new Date(isoString).getTime();
    if (!Number.isFinite(timestamp)) return null;
    return Math.max(0, Math.floor((nowMs - timestamp) / 1000));
  }

  function formatTelemetryFreshness(state = {}, nowMs = Date.now()) {
    if (!state.running) return { label: 'bridge stopped', status: 'muted' };
    if (!state.serialConnected)
      return { label: 'serial disconnected', status: 'error' };
    if (!state.lastTelemetryAt)
      return { label: 'waiting for telemetry', status: 'warn' };
    const age = ageSeconds(state.lastTelemetryAt, nowMs);
    if (age === null) return { label: 'timestamp invalid', status: 'error' };
    if (age <= 3) return { label: 'live', status: 'ok' };
    if (age <= 10) return { label: `delayed · ${age}s`, status: 'warn' };
    return { label: `stale · ${age}s`, status: 'error' };
  }

  function describeConfigValidation(validation = {}) {
    const status = validation?.status || 'pending';
    if (status === 'verified') {
      return { label: 'verified', status: 'ok', recoveryRequired: false };
    }
    if (status === 'invalid') {
      const count = Array.isArray(validation?.errors)
        ? validation.errors.length
        : 0;
      return {
        label: count
          ? `invalid · ${count} error${count === 1 ? '' : 's'}`
          : 'invalid',
        status: 'error',
        recoveryRequired: true,
      };
    }
    return { label: 'pending', status: 'warn', recoveryRequired: false };
  }

  function describeAuthority(session = {}) {
    const authority = session?.deviceAuthority || 'pending';
    const labels = {
      verified: 'verified device truth',
      'verified-device-different': 'device differs from draft',
      uncertain: 'apply outcome uncertain',
      resynchronizing: 'resynchronizing',
      preflighting: 'checking candidate',
      pending: 'pending',
    };
    const status =
      authority === 'verified'
        ? 'ok'
        : authority === 'pending' ||
            authority === 'preflighting' ||
            authority === 'resynchronizing'
          ? 'warn'
          : 'error';
    return {
      label: labels[authority] || authority,
      status,
      recoveryRequired: ![
        'verified',
        'pending',
        'preflighting',
        'resynchronizing',
      ].includes(authority),
    };
  }

  function describeDraft(session = {}) {
    const authority = describeAuthority(session);
    if (authority.recoveryRequired) return authority;
    if (session?.dirty || session?.draftState === 'dirty') {
      return { label: 'draft staged', status: 'warn', recoveryRequired: true };
    }
    return { label: 'clean', status: 'ok', recoveryRequired: false };
  }

  function activeAlerts(state = {}) {
    return Array.isArray(state?.alerts?.active) ? state.alerts.active : [];
  }

  function describeRouteHeartbeats(routes = [], nowMs = Date.now()) {
    const latest = {};
    routes.forEach((route) => {
      const lane = ROUTE_LANES[route?.flow];
      if (!lane || String(route?.kind || '').startsWith('drop_')) return;
      const timestamp =
        Number(route?.hostTimestampMs) || new Date(route?.at).getTime();
      if (!Number.isFinite(timestamp)) return;
      if (!latest[lane] || timestamp > latest[lane].timestamp) {
        latest[lane] = { route, timestamp };
      }
    });
    return Object.fromEntries(
      Object.values(ROUTE_LANES).map((lane) => {
        const entry = latest[lane];
        if (!entry)
          return [lane, { label: 'not seen', status: 'muted', recent: false }];
        const age = Math.max(0, Math.floor((nowMs - entry.timestamp) / 1000));
        if (age <= 3)
          return [lane, { label: 'active now', status: 'ok', recent: true }];
        if (age < 60)
          return [
            lane,
            { label: `seen ${age}s ago`, status: 'muted', recent: false },
          ];
        return [
          lane,
          {
            label: `seen ${Math.floor(age / 60)}m ago`,
            status: 'muted',
            recent: false,
          },
        ];
      }),
    );
  }

  function describeRoutingDestinations(config = {}) {
    const oscName =
      shortString(config.oscDestinationName, 80) || 'OSC destination';
    const midiName =
      shortString(config.midiDestinationName, 80) || 'MIDI destination';
    const oscHost = shortString(config.oscHost, 256) || '127.0.0.1';
    const oscPort = integerInRange(config.oscPort, 1, 65535, 9000);
    const midiPort = shortString(config.midiLabel, 256) || 'MN42 Bridge';
    return {
      deviceOsc: `${oscName} · OSC ${oscHost}:${oscPort}`,
      deviceMidi: `${midiName} · MIDI ${midiPort}`,
      oscDevice: `${oscName} · OSC input`,
      midiDevice: `${midiName} · MIDI input`,
    };
  }

  function parseSlotTelemetryLine(line) {
    try {
      const payload = JSON.parse(String(line || '').trim());
      if (!Array.isArray(payload?.slots)) return null;
      return {
        slots: payload.slots.map((value) => Number(value)),
        traceId: payload.traceId || payload.trace_id || null,
      };
    } catch (_) {
      return null;
    }
  }

  function changedSlotIndices(previous = [], current = []) {
    if (!Array.isArray(previous) || !Array.isArray(current)) return [];
    const count = Math.min(previous.length, current.length);
    const changed = [];
    for (let index = 0; index < count; index += 1) {
      const before = Number(previous[index]);
      const after = Number(current[index]);
      if (Number.isFinite(before) && Number.isFinite(after) && before !== after)
        changed.push(index);
    }
    return changed;
  }

  function observedSoundcheckLanes(routes = [], detection = {}) {
    const observed = new Set();
    routes.forEach((route) => {
      if (!['serial->osc', 'serial->midi'].includes(route?.flow)) return;
      if (String(route?.kind || '').startsWith('drop_')) return;
      if (route?.kind && route.kind !== 'telemetry') return;
      const traceMatches =
        detection.traceId && route?.traceId === detection.traceId;
      const timestamp =
        Number(route?.hostTimestampMs) || new Date(route?.at).getTime();
      const timeMatches =
        !detection.traceId &&
        Number.isFinite(timestamp) &&
        timestamp >=
          Number(detection.detectedAt || detection.startedAt || 0) - 1000;
      if (traceMatches || timeMatches) observed.add(ROUTE_LANES[route.flow]);
    });
    return observed;
  }

  function parseOutboundRouteDraft(raw) {
    const parsed = JSON.parse(raw);
    if (!Array.isArray(parsed) || parsed.length > 128) {
      throw new Error('Provide an array of up to 128 routes.');
    }
    const ids = new Set();
    return parsed.map((entry, index) => {
      const route = normalizeOutboundMidiMapping(entry, index);
      if (
        !route ||
        [
          entry.sourceIndex ?? entry.index,
          entry.channel,
          entry.controller ?? entry.cc,
        ].some((value) => value == null || String(value).trim() === '')
      ) {
        throw new Error(
          `Route ${index + 1}: choose a source, index 0–127, channel 1–16, and CC 0–127.`,
        );
      }
      if (ids.has(route.id))
        throw new Error(`Route ${index + 1}: route IDs must be unique.`);
      ids.add(route.id);
      return route;
    });
  }

  function expectedSoundcheckLanes(config = {}, slotIndices = []) {
    const expected = ['deviceOsc'];
    if (
      config.midiTelemetryMode !== 'mapped' ||
      (config.outboundMidiMappings || []).some(
        (route) =>
          route.source === 'slots' && slotIndices.includes(route.sourceIndex),
      )
    ) {
      expected.push('deviceMidi');
    }
    return expected;
  }

  function isActionVisibleInMode(modeList, mode) {
    return String(modeList || '')
      .split(/\s+/)
      .filter(Boolean)
      .includes(mode);
  }

  function operatorConfirmationMessage(action) {
    const messages = {
      stop: 'Stop the Bridge? OSC/MIDI routing and the device session will disconnect.',
      clearAlerts:
        'Clear active alerts? This acknowledges the current list; unresolved conditions may raise again.',
    };
    return messages[action] || '';
  }

  function latestLearnableMidiCc(routes = [], afterMs = 0) {
    for (let index = routes.length - 1; index >= 0; index -= 1) {
      const route = routes[index];
      if (route?.flow !== 'midi->serial') continue;
      if (
        !['command', 'drop_invalid', 'drop_parameter_control'].includes(
          route?.kind,
        )
      ) {
        continue;
      }
      const observedAt =
        Number(route?.hostTimestampMs) || new Date(route?.at).getTime();
      if (!Number.isFinite(observedAt) || observedAt < afterMs) continue;
      const status = Number(route?.status);
      const controller = Number(route?.slot);
      const value = Number(route?.value);
      if (!Number.isInteger(status) || (status & 0xf0) !== 0xb0) continue;
      if (!Number.isInteger(controller) || controller < 0 || controller > 127) {
        continue;
      }
      return {
        channel: (status & 0x0f) + 1,
        controller,
        value: Number.isFinite(value) ? value : null,
        observedAt,
      };
    }
    return null;
  }

  function recentOscAddresses(routes = [], limit = 8) {
    const addresses = [];
    const seen = new Set();
    for (let index = routes.length - 1; index >= 0; index -= 1) {
      const route = routes[index];
      if (!String(route?.flow || '').endsWith('->osc')) continue;
      const address = String(route?.address || '').trim();
      if (
        !address.startsWith('/') ||
        address.startsWith('/mn42/') ||
        seen.has(address)
      ) {
        continue;
      }
      seen.add(address);
      addresses.push(address);
      if (addresses.length >= limit) break;
    }
    return addresses;
  }

  return {
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
  };
});
