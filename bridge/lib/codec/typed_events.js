const MAX_EVENT_JSON_LEN = 4096;
const OSC_EVENT_PREFIX = '/mn42/event/';

function normalizeIntegerInRange(raw, min, max) {
  if (raw === null || raw === undefined) return null;
  const value =
    typeof raw === 'number'
      ? Math.trunc(raw)
      : Number.parseInt(String(raw).trim(), 10);
  if (!Number.isInteger(value) || value < min || value > max) return null;
  return value;
}

function normalizeChannel(raw, fallback = 1) {
  const normalized = normalizeIntegerInRange(raw, 1, 16);
  return normalized === null ? fallback : normalized;
}

function normalizePitchBend(raw) {
  if (raw === null || raw === undefined) return 8192;
  if (typeof raw === 'number' && Number.isFinite(raw)) {
    const truncated = Math.trunc(raw);
    if (truncated >= -8192 && truncated <= 8191) return truncated + 8192;
    if (truncated >= 0 && truncated <= 16383) return truncated;
  }
  const parsed = Number.parseInt(String(raw).trim(), 10);
  if (!Number.isInteger(parsed)) return null;
  if (parsed >= -8192 && parsed <= 8191) return parsed + 8192;
  if (parsed >= 0 && parsed <= 16383) return parsed;
  return null;
}

function normalizeByteArray(raw, { sevenBit = false } = {}) {
  const source = Array.isArray(raw) ? raw : null;
  if (!source || !source.length) return null;
  const max = sevenBit ? 127 : 255;
  const normalized = [];
  for (const value of source) {
    const byte = normalizeIntegerInRange(value, 0, max);
    if (byte === null) return null;
    normalized.push(byte);
  }
  return normalized;
}

function normalizeEventKind(rawKind) {
  if (typeof rawKind !== 'string') return null;
  const kind = rawKind.trim().toLowerCase();
  switch (kind) {
    case 'cc':
      return 'cc';
    case 'note_on':
    case 'note_onoff':
    case 'noteon':
      return 'note_on';
    case 'note_off':
    case 'noteoff':
      return 'note_off';
    case 'pitch_bend':
    case 'pitchbend':
      return 'pitch_bend';
    case 'channel_aftertouch':
    case 'channel_pressure':
    case 'aftertouch':
      return 'channel_aftertouch';
    case 'poly_aftertouch':
    case 'poly_pressure':
      return 'poly_aftertouch';
    case 'program_change':
    case 'program':
      return 'program_change';
    case 'nrpn':
      return 'nrpn';
    case 'rpn':
      return 'rpn';
    case 'sysex':
      return 'sysex';
    default:
      return null;
  }
}

function inferEventKindFromAddress(address) {
  if (typeof address !== 'string' || !address.startsWith(OSC_EVENT_PREFIX)) {
    return null;
  }
  const suffix = address.slice(OSC_EVENT_PREFIX.length);
  return normalizeEventKind(suffix);
}

function parseOscPayloadArg(
  rawArg,
  { maxJsonLength = MAX_EVENT_JSON_LEN } = {},
) {
  if (typeof rawArg === 'string') {
    if (rawArg.length > maxJsonLength) return null;
    try {
      const parsed = JSON.parse(rawArg);
      return parsed && typeof parsed === 'object' ? parsed : null;
    } catch (_) {
      return null;
    }
  }
  if (rawArg && typeof rawArg === 'object' && !Array.isArray(rawArg)) {
    return rawArg;
  }
  return null;
}

function normalizeTypedEvent(kind, payload = {}) {
  const normalizedKind = normalizeEventKind(kind);
  if (!normalizedKind || !payload || typeof payload !== 'object') return null;
  const channel = normalizeChannel(payload.channel, 1);
  switch (normalizedKind) {
    case 'cc': {
      const controller = normalizeIntegerInRange(
        payload.controller ?? payload.cc,
        0,
        127,
      );
      const value = normalizeIntegerInRange(payload.value, 0, 127);
      if (controller === null || value === null) return null;
      return { kind: 'cc', channel, controller, value };
    }
    case 'note_on':
    case 'note_off': {
      const note = normalizeIntegerInRange(payload.note, 0, 127);
      const velocity = normalizeIntegerInRange(payload.velocity ?? 0, 0, 127);
      if (note === null || velocity === null) return null;
      return { kind: normalizedKind, channel, note, velocity };
    }
    case 'pitch_bend': {
      const value = normalizePitchBend(payload.value ?? payload.value14);
      if (value === null) return null;
      return { kind: 'pitch_bend', channel, value };
    }
    case 'channel_aftertouch': {
      const pressure = normalizeIntegerInRange(
        payload.pressure ?? payload.value,
        0,
        127,
      );
      if (pressure === null) return null;
      return { kind: 'channel_aftertouch', channel, pressure };
    }
    case 'poly_aftertouch': {
      const note = normalizeIntegerInRange(payload.note, 0, 127);
      const pressure = normalizeIntegerInRange(
        payload.pressure ?? payload.value,
        0,
        127,
      );
      if (note === null || pressure === null) return null;
      return { kind: 'poly_aftertouch', channel, note, pressure };
    }
    case 'program_change': {
      const program = normalizeIntegerInRange(
        payload.program ?? payload.value,
        0,
        127,
      );
      if (program === null) return null;
      return { kind: 'program_change', channel, program };
    }
    case 'nrpn':
    case 'rpn': {
      const parameter = normalizeIntegerInRange(
        payload.parameter ?? payload.param,
        0,
        16383,
      );
      const value = normalizeIntegerInRange(payload.value, 0, 16383);
      if (parameter === null || value === null) return null;
      return { kind: normalizedKind, channel, parameter, value };
    }
    case 'sysex': {
      const bytes = normalizeByteArray(payload.bytes, { sevenBit: false });
      if (!bytes || bytes.length < 2) return null;
      if (bytes[0] !== 0xf0 || bytes[bytes.length - 1] !== 0xf7) return null;
      return { kind: 'sysex', bytes };
    }
    default:
      return null;
  }
}

function eventAddressForKind(kind) {
  return `${OSC_EVENT_PREFIX}${kind}`;
}

function typedEventToMidiPackets(event) {
  if (!event || typeof event !== 'object') return [];
  const channelNibble =
    event.kind === 'sysex'
      ? 0
      : normalizeIntegerInRange(event.channel, 1, 16) - 1;
  switch (event.kind) {
    case 'cc':
      return [[0xb0 | channelNibble, event.controller, event.value]];
    case 'note_on':
      return [[0x90 | channelNibble, event.note, event.velocity]];
    case 'note_off':
      return [[0x80 | channelNibble, event.note, event.velocity]];
    case 'pitch_bend': {
      const value = normalizePitchBend(event.value);
      if (value === null) return [];
      const lsb = value & 0x7f;
      const msb = (value >> 7) & 0x7f;
      return [[0xe0 | channelNibble, lsb, msb]];
    }
    case 'channel_aftertouch':
      return [[0xd0 | channelNibble, event.pressure]];
    case 'poly_aftertouch':
      return [[0xa0 | channelNibble, event.note, event.pressure]];
    case 'program_change':
      return [[0xc0 | channelNibble, event.program]];
    case 'nrpn':
    case 'rpn': {
      const parameter = normalizeIntegerInRange(event.parameter, 0, 16383);
      const value = normalizeIntegerInRange(event.value, 0, 16383);
      if (parameter === null || value === null) return [];
      const paramMsb = (parameter >> 7) & 0x7f;
      const paramLsb = parameter & 0x7f;
      const valueMsb = (value >> 7) & 0x7f;
      const valueLsb = value & 0x7f;
      return event.kind === 'nrpn'
        ? [
            [0xb0 | channelNibble, 99, paramMsb],
            [0xb0 | channelNibble, 98, paramLsb],
            [0xb0 | channelNibble, 6, valueMsb],
            [0xb0 | channelNibble, 38, valueLsb],
          ]
        : [
            [0xb0 | channelNibble, 101, paramMsb],
            [0xb0 | channelNibble, 100, paramLsb],
            [0xb0 | channelNibble, 6, valueMsb],
            [0xb0 | channelNibble, 38, valueLsb],
          ];
    }
    case 'sysex':
      return [event.bytes];
    default:
      return [];
  }
}

function parseMidiMessageToTypedEvents(arr, channelRpnState = new Map()) {
  if (!Array.isArray(arr) || !arr.length) return [];
  const bytes = normalizeByteArray(arr, { sevenBit: false });
  if (!bytes) return [];

  if (bytes[0] === 0xf0 && bytes[bytes.length - 1] === 0xf7) {
    return [{ kind: 'sysex', bytes }];
  }

  const status = bytes[0];
  const kindNibble = status & 0xf0;
  const channel = (status & 0x0f) + 1;

  if (kindNibble === 0x80 && bytes.length >= 3) {
    return [
      {
        kind: 'note_off',
        channel,
        note: bytes[1],
        velocity: bytes[2],
      },
    ];
  }
  if (kindNibble === 0x90 && bytes.length >= 3) {
    if (bytes[2] === 0) {
      return [{ kind: 'note_off', channel, note: bytes[1], velocity: 0 }];
    }
    return [{ kind: 'note_on', channel, note: bytes[1], velocity: bytes[2] }];
  }
  if (kindNibble === 0xa0 && bytes.length >= 3) {
    return [
      {
        kind: 'poly_aftertouch',
        channel,
        note: bytes[1],
        pressure: bytes[2],
      },
    ];
  }
  if (kindNibble === 0xc0 && bytes.length >= 2) {
    return [{ kind: 'program_change', channel, program: bytes[1] }];
  }
  if (kindNibble === 0xd0 && bytes.length >= 2) {
    return [{ kind: 'channel_aftertouch', channel, pressure: bytes[1] }];
  }
  if (kindNibble === 0xe0 && bytes.length >= 3) {
    const value = ((bytes[2] & 0x7f) << 7) | (bytes[1] & 0x7f);
    return [{ kind: 'pitch_bend', channel, value }];
  }
  if (kindNibble === 0xb0 && bytes.length >= 3) {
    const controller = bytes[1];
    const value = bytes[2];
    const events = [{ kind: 'cc', channel, controller, value }];

    const state = channelRpnState.get(channel) || {
      mode: null,
      nrpnMsb: null,
      nrpnLsb: null,
      rpnMsb: null,
      rpnLsb: null,
      dataMsb: null,
      dataLsb: null,
    };

    if (controller === 99) {
      state.mode = 'nrpn';
      state.nrpnMsb = value;
    } else if (controller === 98) {
      state.mode = 'nrpn';
      state.nrpnLsb = value;
    } else if (controller === 101) {
      state.mode = 'rpn';
      state.rpnMsb = value;
    } else if (controller === 100) {
      state.mode = 'rpn';
      state.rpnLsb = value;
    } else if (controller === 6) {
      state.dataMsb = value;
    } else if (controller === 38) {
      state.dataLsb = value;
    }
    channelRpnState.set(channel, state);

    if (
      (controller === 6 || controller === 38) &&
      state.mode &&
      state.dataMsb !== null
    ) {
      const isNrpn = state.mode === 'nrpn';
      const paramMsb = isNrpn ? state.nrpnMsb : state.rpnMsb;
      const paramLsb = isNrpn ? state.nrpnLsb : state.rpnLsb;
      if (paramMsb !== null && paramLsb !== null) {
        const parameter = ((paramMsb & 0x7f) << 7) | (paramLsb & 0x7f);
        const combined = ((state.dataMsb & 0x7f) << 7) | (state.dataLsb || 0);
        if (parameter !== 0x3fff) {
          events.push({
            kind: isNrpn ? 'nrpn' : 'rpn',
            channel,
            parameter,
            value: combined,
          });
        }
      }
    }
    return events;
  }
  return [];
}

function normalizeOscTypedEventMessage(msg) {
  if (
    !msg ||
    typeof msg !== 'object' ||
    typeof msg.address !== 'string' ||
    !msg.address.startsWith(OSC_EVENT_PREFIX)
  ) {
    return null;
  }
  const kind = inferEventKindFromAddress(msg.address);
  if (!kind) return null;

  const payloadArg =
    Array.isArray(msg.args) && msg.args.length > 0 ? msg.args[0] : null;
  const payload = parseOscPayloadArg(payloadArg);
  if (!payload) return null;

  const event = normalizeTypedEvent(kind, payload);
  if (!event) return null;

  return {
    event,
    payload,
    traceId: payload.traceId,
    sourceTimestampMs: payload.timestampMs,
  };
}

function buildSlotCommandFromCcEvent(event) {
  if (!event || event.kind !== 'cc') return null;
  const slot = normalizeIntegerInRange(event.controller, 0, 41);
  const value = normalizeIntegerInRange(event.value, 0, 127);
  if (slot === null || value === null) return null;
  return { cmd: 'SET_SLOT_VALUE', slot, value };
}

module.exports = {
  MAX_EVENT_JSON_LEN,
  OSC_EVENT_PREFIX,
  normalizeIntegerInRange,
  normalizeChannel,
  normalizePitchBend,
  normalizeByteArray,
  normalizeEventKind,
  inferEventKindFromAddress,
  parseOscPayloadArg,
  normalizeTypedEvent,
  eventAddressForKind,
  typedEventToMidiPackets,
  parseMidiMessageToTypedEvents,
  normalizeOscTypedEventMessage,
  buildSlotCommandFromCcEvent,
};
