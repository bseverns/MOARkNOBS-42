const { midiEchoKey } = require('../observability/performance');

function createFeedbackGuard({
  getAllowFeedbackLoops,
  getFeedbackWindowMs,
  now = () => Date.now(),
} = {}) {
  const recentTelemetryMidi = new Map();

  function pruneTelemetryMarkers(nowMs = now()) {
    const maxAge = Math.max(1, Number(getFeedbackWindowMs?.()) || 1);
    for (const [key, seenAt] of recentTelemetryMidi.entries()) {
      if (nowMs - seenAt > maxAge) recentTelemetryMidi.delete(key);
    }
  }

  function markTelemetryMidi(status, cc, value) {
    if (getAllowFeedbackLoops?.()) return;
    pruneTelemetryMarkers();
    recentTelemetryMidi.set(midiEchoKey(status, cc, value), now());
  }

  function shouldSuppressMidiEcho(status, cc, value) {
    if (getAllowFeedbackLoops?.()) return false;
    const nowMs = now();
    pruneTelemetryMarkers(nowMs);
    const seenAt = recentTelemetryMidi.get(midiEchoKey(status, cc, value));
    if (!seenAt) return false;
    return nowMs - seenAt <= getFeedbackWindowMs?.();
  }

  function clear() {
    recentTelemetryMidi.clear();
  }

  return {
    markTelemetryMidi,
    shouldSuppressMidiEcho,
    clear,
  };
}

module.exports = {
  createFeedbackGuard,
};
