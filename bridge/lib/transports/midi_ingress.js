function createMidiMessageHandler({
  nextTraceId,
  now,
  shouldSuppressMidiEcho,
  bumpCounter,
  recordRoute,
  parseMidiMessageToTypedEvents,
  midiRpnStateByChannel,
  sendTypedEventToOsc,
  buildSlotCommandFromCcEvent,
  pushLog,
  queuePendingCommand,
  sendLine,
  formatLiveValueCommand,
}) {
  const timestampNow = typeof now === 'function' ? now : () => Date.now();

  return function handleMidiMessage(msg) {
    if (!msg || typeof msg.toArray !== 'function') return;
    const arr = msg.toArray();
    const midiTraceId = nextTraceId('midi');
    const hostTimestampMs = timestampNow();
    const isCcMessage =
      Array.isArray(arr) &&
      arr.length >= 3 &&
      Number.isInteger(arr[0]) &&
      (arr[0] & 0xf0) === 0xb0 &&
      Number.isInteger(arr[1]) &&
      Number.isInteger(arr[2]);

    if (isCcMessage && shouldSuppressMidiEcho(arr[0], arr[1], arr[2])) {
      bumpCounter('feedbackSuppressed');
      recordRoute({
        flow: 'midi->serial',
        kind: 'drop_feedback',
        status: arr[0],
        slot: arr[1],
        value: arr[2],
        traceId: midiTraceId,
        reason: 'feedback_window',
        hostTimestampMs,
      });
      return;
    }

    const typedEvents = parseMidiMessageToTypedEvents(
      arr,
      midiRpnStateByChannel,
    );
    if (!typedEvents.length) return;
    typedEvents.forEach((event) => {
      sendTypedEventToOsc(event, {
        flow: 'midi->osc',
        kind: 'event',
        reason: event.kind,
        traceId: midiTraceId,
        hostTimestampMs,
      });
    });

    const ccEvent = typedEvents.find((event) => event.kind === 'cc');
    if (!ccEvent) return;
    const cmd = buildSlotCommandFromCcEvent(ccEvent);
    if (!cmd) {
      bumpCounter('badMidiCmdDrops');
      recordRoute({
        flow: 'midi->serial',
        kind: 'drop_invalid',
        status: arr[0],
        slot: ccEvent.controller,
        value: ccEvent.value,
        traceId: midiTraceId,
        hostTimestampMs,
      });
      pushLog('warn', 'dropping bad MIDI CC', arr);
      return;
    }

    recordRoute({
      flow: 'midi->serial',
      kind: 'command',
      status: arr[0],
      slot: cmd.slot,
      value: cmd.value,
      traceId: midiTraceId,
      hostTimestampMs,
    });
    queuePendingCommand({
      slot: cmd.slot,
      value: cmd.value,
      traceId: midiTraceId,
      hostTimestampMs,
      source: 'midi',
    });
    sendLine(formatLiveValueCommand(cmd));
  };
}

module.exports = {
  createMidiMessageHandler,
};
