(function attachOperatorState(root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.MN42BridgeOperatorState = api;
})(typeof globalThis === 'object' ? globalThis : this, () => {
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

  return {
    activeAlerts,
    describeAuthority,
    describeConfigValidation,
    describeDraft,
    formatTelemetryFreshness,
  };
});
