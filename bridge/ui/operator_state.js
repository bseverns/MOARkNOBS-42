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

  function ageSeconds(isoString, nowMs) {
    const timestamp = new Date(isoString).getTime();
    if (!Number.isFinite(timestamp)) return null;
    return Math.max(0, Math.floor((nowMs - timestamp) / 1000));
  }

  function formatTelemetryFreshness(state = {}, nowMs = Date.now()) {
    if (!state.running) return { label: 'bridge stopped', status: 'muted' };
    if (!state.serialConnected) return { label: 'serial disconnected', status: 'error' };
    if (!state.lastTelemetryAt) return { label: 'waiting for telemetry', status: 'warn' };
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
      const count = Array.isArray(validation?.errors) ? validation.errors.length : 0;
      return {
        label: count ? `invalid · ${count} error${count === 1 ? '' : 's'}` : 'invalid',
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
    const status = authority === 'verified'
      ? 'ok'
      : authority === 'pending' || authority === 'preflighting' || authority === 'resynchronizing'
        ? 'warn'
        : 'error';
    return {
      label: labels[authority] || authority,
      status,
      recoveryRequired: !['verified', 'pending', 'preflighting', 'resynchronizing'].includes(authority),
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
      if (!lane) return;
      const timestamp = Number(route?.hostTimestampMs) || new Date(route?.at).getTime();
      if (!Number.isFinite(timestamp)) return;
      if (!latest[lane] || timestamp > latest[lane].timestamp) {
        latest[lane] = { route, timestamp };
      }
    });
    return Object.fromEntries(
      Object.values(ROUTE_LANES).map((lane) => {
        const entry = latest[lane];
        if (!entry) return [lane, { label: 'not seen', status: 'muted', recent: false }];
        const age = Math.max(0, Math.floor((nowMs - entry.timestamp) / 1000));
        if (age <= 3) return [lane, { label: 'active now', status: 'ok', recent: true }];
        if (age < 60) return [lane, { label: `seen ${age}s ago`, status: 'muted', recent: false }];
        return [
          lane,
          { label: `seen ${Math.floor(age / 60)}m ago`, status: 'muted', recent: false },
        ];
      }),
    );
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
      if (Number.isFinite(before) && Number.isFinite(after) && before !== after) changed.push(index);
    }
    return changed;
  }

  function observedSoundcheckLanes(routes = [], detection = {}) {
    const observed = new Set();
    routes.forEach((route) => {
      if (!['serial->osc', 'serial->midi'].includes(route?.flow)) return;
      const traceMatches = detection.traceId && route?.traceId === detection.traceId;
      const timestamp = Number(route?.hostTimestampMs) || new Date(route?.at).getTime();
      const timeMatches = !detection.traceId && Number.isFinite(timestamp) &&
        timestamp >= Number(detection.detectedAt || detection.startedAt || 0) - 1000;
      if (traceMatches || timeMatches) observed.add(ROUTE_LANES[route.flow]);
    });
    return observed;
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
      if (
        !Number.isInteger(controller) ||
        controller < 0 ||
        controller > 127
      ) {
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
    formatTelemetryFreshness,
    isActionVisibleInMode,
    latestLearnableMidiCc,
    observedSoundcheckLanes,
    operatorConfirmationMessage,
    parseSlotTelemetryLine,
    recentOscAddresses,
  };
});
