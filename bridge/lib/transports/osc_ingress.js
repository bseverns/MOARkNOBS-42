function createOscMessageHandler({
  oscCmdAddress,
  oscEventPrefix,
  maxCmdLen,
  validateCmd,
  formatLiveValueCommand,
  normalizeOscTypedEventMessage,
  buildSlotCommandFromCcEvent,
  sendTypedEventToMidi,
  queuePendingCommand,
  sendLine,
  nextTraceId,
  extractTraceId,
  extractTimestampMs,
  bumpCounter,
  recordRoute,
  pushLog,
  now,
}) {
  const timestampNow = typeof now === 'function' ? now : () => Date.now();

  function handleOscCommandPayload(msg) {
    if (!Array.isArray(msg.args) || !msg.args.length) return;
    let data = msg.args[0];
    if (typeof data === 'string') {
      if (data.length > maxCmdLen) {
        bumpCounter('badOscCmdDrops');
        pushLog('warn', 'OSC cmd too big');
        return;
      }
      try {
        data = JSON.parse(data);
      } catch (_) {
        bumpCounter('badOscCmdDrops');
        pushLog('warn', 'bad OSC JSON');
        return;
      }
    }

    const cmd = validateCmd(data);
    if (!cmd) {
      bumpCounter('badOscCmdDrops');
      recordRoute({
        flow: 'osc->serial',
        kind: 'drop_invalid',
        address: oscCmdAddress,
        traceId: extractTraceId(data) || nextTraceId('osc'),
        sourceTimestampMs: extractTimestampMs(data),
        hostTimestampMs: timestampNow(),
      });
      pushLog('warn', 'bad OSC cmd', data);
      return;
    }

    const hostTimestampMs = timestampNow();
    const traceId = extractTraceId(data) || nextTraceId('osc');
    const sourceTimestampMs = extractTimestampMs(data);
    recordRoute({
      flow: 'osc->serial',
      kind: 'command',
      address: oscCmdAddress,
      slot: cmd.slot,
      value: cmd.value,
      traceId,
      sourceTimestampMs,
      hostTimestampMs,
    });
    queuePendingCommand({
      slot: cmd.slot,
      value: cmd.value,
      traceId,
      hostTimestampMs,
      source: 'osc',
    });
    sendLine(formatLiveValueCommand(cmd));
  }

  function handleOscTypedEventPayload(msg) {
    const isTypedEventAddress =
      typeof msg.address === 'string' && msg.address.startsWith(oscEventPrefix);
    const typed = normalizeOscTypedEventMessage(msg);
    if (!typed) {
      if (isTypedEventAddress) {
        bumpCounter('badOscCmdDrops');
        recordRoute({
          flow: 'osc->midi',
          kind: 'drop_invalid',
          address: msg.address,
          traceId: nextTraceId('osc-event'),
          hostTimestampMs: timestampNow(),
          reason: 'bad_event_payload',
        });
        pushLog('warn', 'bad OSC event payload', msg.args?.[0]);
      }
      return;
    }

    const hostTimestampMs = timestampNow();
    const traceId = typed.traceId || nextTraceId('osc-event');
    const sourceTimestampMs = typed.sourceTimestampMs;
    const eventSent = sendTypedEventToMidi(typed.event, {
      flow: 'osc->midi',
      kind: 'event',
      reason: typed.event.kind,
      traceId,
      sourceTimestampMs,
      hostTimestampMs,
    });
    if (!eventSent) {
      bumpCounter('badOscCmdDrops');
      recordRoute({
        flow: 'osc->midi',
        kind: 'drop_invalid',
        address: typed.address,
        traceId,
        sourceTimestampMs,
        hostTimestampMs,
        reason: 'event_not_sent',
      });
      pushLog('warn', 'bad OSC event payload', msg.args[0]);
      return;
    }

    const cmd = buildSlotCommandFromCcEvent(typed.event);
    if (!cmd) return;
    recordRoute({
      flow: 'osc->serial',
      kind: 'command',
      address: typed.address,
      slot: cmd.slot,
      value: cmd.value,
      traceId,
      sourceTimestampMs,
      hostTimestampMs,
    });
    queuePendingCommand({
      slot: cmd.slot,
      value: cmd.value,
      traceId,
      hostTimestampMs,
      source: 'osc_event',
    });
    sendLine(formatLiveValueCommand(cmd));
  }

  return function handleOscMessage(msg) {
    if (!msg || typeof msg !== 'object') return;
    if (msg.address === oscCmdAddress) {
      handleOscCommandPayload(msg);
      return;
    }
    handleOscTypedEventPayload(msg);
  };
}

module.exports = {
  createOscMessageHandler,
};
