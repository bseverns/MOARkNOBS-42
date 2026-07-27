function createMidiMessageHandler({
  nextTraceId,
  now,
  shouldSuppressMidiEcho,
  bumpCounter,
  recordRoute,
  parseMidiMessageToTypedEvents,
  midiRpnStateByChannel,
  sendTypedEventToOsc,
  sendMappedMidiEventToOsc,
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
      sendMappedMidiEventToOsc?.(event, {
        flow: 'midi->osc',
        kind: 'mapping',
        reason: event.kind,
        traceId: midiTraceId,
        hostTimestampMs,
      });
    });

    const ccEvent = typedEvents.find((event) => event.kind === 'cc');
    if (!ccEvent) return;
    // RPN/NRPN selectors and data-entry messages are control-sequence traffic,
    // not implicit MN42 slot commands. Without this guard CC6/38 can change a
    // slot while a host is merely completing a 14-bit parameter message.
    if ([6, 38, 98, 99, 100, 101].includes(ccEvent.controller)) {
      recordRoute({
        flow: 'midi->serial',
        kind: 'drop_parameter_control',
        status: arr[0],
        slot: ccEvent.controller,
        value: ccEvent.value,
        traceId: midiTraceId,
        hostTimestampMs,
      });
      return;
    }
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
    try {
      sendLine(formatLiveValueCommand(cmd));
      queuePendingCommand({
        slot: cmd.slot,
        value: cmd.value,
        traceId: midiTraceId,
        hostTimestampMs,
        source: 'midi',
      });
    } catch (err) {
      bumpCounter('badMidiCmdDrops');
      pushLog('warn', `MIDI command dropped: serial unavailable (${err.message || err})`);
    }
  };
}

module.exports = {
  createMidiMessageHandler,
};
