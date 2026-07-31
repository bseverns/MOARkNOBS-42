const { EventEmitter } = require('node:events');

function clone(value) {
  return value == null ? value : JSON.parse(JSON.stringify(value));
}

function createDefaultManifest(overrides = {}) {
  return {
    device_name: 'MOARkNOBS-42',
    fw_version: 'sim-fw',
    git_sha: 'simulated',
    build_time: '2026-05-24T00:00:00Z',
    schema_version: 7,
    slot_count: 42,
    pot_count: 42,
    envelope_count: 6,
    arg_method_count: 14,
    led_count: 52,
    power_profile: 'POWER_CHOKED_V1',
    led_brightness_cap: 26,
    rail_topology_verified: false,
    display_present: true,
    display_ok: true,
    display_init_failures: 0,
    display_status: 'ok',
    free_ram: 32768,
    free_flash: 1024 * 1024,
    brownout_count: 0,
    eeprom_primary_valid: true,
    eeprom_backup_valid: true,
    eeprom_last_load: 'primary',
    capabilities: {
      profile_save: true,
      profile_load: true,
      profile_reset: true,
      macro_snapshot: false,
      scenes: false,
      arp_live: true,
    },
    ...overrides,
  };
}

function createDefaultSchema(overrides = {}) {
  return {
    type: 'object',
    schema_version: 7,
    required: ['slots', 'efSlots', 'filter', 'arg', 'led'],
    properties: {
      slots: { type: 'array', items: { type: 'object' } },
      efSlots: { type: 'array', items: { type: 'object' } },
      filter: { type: 'object' },
      arg: { type: 'object' },
      led: { type: 'object' },
    },
    ...overrides,
  };
}

function createDefaultConfig(manifest = createDefaultManifest()) {
  return {
    fw_version: manifest.fw_version,
    schema_version: manifest.schema_version,
    pots: Array.from({ length: manifest.pot_count }, (_, index) => ({
      index,
      channel: (index % 16) + 1,
      cc: index % 128,
    })),
    slots: Array.from({ length: manifest.slot_count }, (_, index) => ({
      index,
      type: 1,
      type_name: 'CC',
      channel: (index % 16) + 1,
      data1: index % 128,
      ef_index: index % manifest.envelope_count,
      ef: {
        index: index % manifest.envelope_count,
        filter_index: 0,
        filter_name: 'LINEAR',
        frequency: 1000,
        q: 0.707,
        oversample: 4,
        smoothing: 0.2,
        baseline: 0,
        gain: 1,
      },
      active: true,
      arpNote: 0,
      sysexTemplate: '',
      ef_payload: {
        type: 0,
        type_name: 'LINEAR',
        freq: 1000,
        q: 0.707,
      },
      arg: {
        enabled: false,
        method: 0,
        method_name: 'PLUS',
        sourceA: 0,
        sourceB: 1,
      },
    })),
    efSlots: Array.from({ length: manifest.envelope_count }, (_, index) => ({
      index,
      slots: [index],
    })),
    filter: {
      type: 'LOWPASS',
      frequency: 800,
      q: 1,
      idleFloor: 0,
    },
    arg: {
      enabled: false,
      method: 'PLUS',
      sourceA: 0,
      sourceB: 1,
    },
    led: {
      brightness: 64,
      rgb: { r: 17, g: 34, b: 51 },
      hex: '#112233',
      mode: 'STATIC',
    },
  };
}

function createSimulatedMn42Device(options = {}) {
  const events = new EventEmitter();
  const manifest = createDefaultManifest(options.manifest);
  const schema = createDefaultSchema(options.schema);
  let config = clone(options.config ?? createDefaultConfig(manifest));
  let ackMode = options.ackMode ?? 'good';
  let ackDelayMs = Number(options.ackDelayMs ?? 0);
  let setAllBuffer = '';
  const defaultArp = {
    active: false,
    slot: 0,
    length_ticks: 12,
    shape: 0,
    shape_name: 'up',
    swing_percent: 0,
    gate_percent: 50,
    octave_range: 0,
    pattern_length: 4,
  };
  const defaultProfile = {
    arp: clone(defaultArp),
    lfos: [],
    routes: [],
  };
  const profiles = Array.from({ length: 4 }, () => clone(defaultProfile));
  const profileChunkBuffers = new Map();
  let activeProfile = 0;
  let arp = clone(defaultArp);
  let connected = true;

  function clamp(value, minimum, maximum, fallback) {
    const number = Number(value);
    return Number.isFinite(number)
      ? Math.max(minimum, Math.min(maximum, Math.round(number)))
      : fallback;
  }

  function profileId(value) {
    return clamp(value, 0, profiles.length - 1, 0);
  }

  function patchProfile(id, patch) {
    const current = profiles[id];
    const next = clone(current);
    for (const [key, value] of Object.entries(patch || {})) {
      if (value && typeof value === 'object' && !Array.isArray(value)) {
        next[key] = { ...(next[key] || {}), ...clone(value) };
      } else {
        next[key] = clone(value);
      }
    }
    next.arp.pattern_length = clamp(
      next.arp.pattern_length,
      2,
      16,
      current.arp.pattern_length,
    );
    profiles[id] = next;
    if (id === activeProfile) {
      arp = { ...arp, ...clone(next.arp) };
    }
  }

  function emitProfileSetReceipt(id) {
    emitJson({
      type: 'response',
      status: 'ok',
      command: 'SET_PROFILE',
      profile: id,
      active_profile: activeProfile,
      profile_updated: true,
      active_applied: id === activeProfile,
    });
  }

  function applyProfilePayload(id, payload) {
    patchProfile(id, JSON.parse(payload));
    emitProfileSetReceipt(id);
  }

  function emitJson(payload) {
    events.emit('line', JSON.stringify(payload));
  }

  function receiveLine(line) {
    if (!connected) return;
    const trimmed = String(line || '').trim();
    if (!trimmed) return;
    if (trimmed === 'HELLO') {
      emitJson({ hello: 'mn42' });
      return;
    }
    if (trimmed === 'GET_MANIFEST') {
      emitJson(manifest);
      return;
    }
    if (trimmed === 'GET_SCHEMA') {
      emitJson(schema);
      return;
    }
    if (trimmed === 'GET_CONFIG') {
      emitJson(config);
      return;
    }
    if (trimmed.startsWith('GET_PROFILE,')) {
      const id = profileId(trimmed.split(',')[1]);
      emitJson({
        type: 'response',
        command: 'GET_PROFILE',
        profile: id,
        active_profile: activeProfile,
        active: id === activeProfile,
        stored: true,
        ...clone(profiles[id]),
      });
      return;
    }
    if (trimmed.startsWith('SET_PROFILE,')) {
      const firstComma = trimmed.indexOf(',');
      const secondComma = trimmed.indexOf(',', firstComma + 1);
      const id = profileId(trimmed.slice(firstComma + 1, secondComma));
      applyProfilePayload(id, trimmed.slice(secondComma + 1));
      return;
    }
    if (trimmed.startsWith('SET_PROFILE_CHUNK,')) {
      const parts = trimmed.split(',', 5);
      const id = profileId(parts[1]);
      const sequence = Number(parts[2]);
      const total = Number(parts[3]);
      const payloadOffset = parts
        .slice(0, 4)
        .reduce((offset, part) => offset + part.length + 1, 0);
      const entry = profileChunkBuffers.get(id) || {
        total,
        chunks: Array.from({ length: total }),
      };
      entry.chunks[sequence] = trimmed.slice(payloadOffset);
      profileChunkBuffers.set(id, entry);
      if (
        entry.chunks.filter((chunk) => chunk !== undefined).length ===
        entry.total
      ) {
        profileChunkBuffers.delete(id);
        applyProfilePayload(id, entry.chunks.join(''));
      }
      return;
    }
    if (trimmed === 'GET_ARP') {
      emitJson({ type: 'response', command: 'GET_ARP', ...clone(arp) });
      return;
    }
    if (trimmed.startsWith('SET_ARP,')) {
      const values = trimmed.split(',').slice(1);
      const shapeNames = [
        'up',
        'down',
        'up_down',
        'random',
        'drunk',
        'euclidean',
      ];
      arp = {
        ...arp,
        length_ticks: clamp(values[0], 1, 24, arp.length_ticks),
        shape: clamp(values[1], 0, 5, arp.shape),
        swing_percent: clamp(values[2], 0, 80, arp.swing_percent),
        gate_percent: clamp(values[3], 5, 100, arp.gate_percent),
        octave_range: clamp(values[4], 0, 3, arp.octave_range),
        pattern_length:
          values.length >= 6
            ? clamp(values[5], 2, 16, arp.pattern_length)
            : arp.pattern_length,
      };
      arp.shape_name = shapeNames[arp.shape] || 'up';
      profiles[activeProfile].arp = clone(arp);
      emitJson({
        type: 'response',
        status: 'ok',
        command: 'SET_ARP',
        ...clone(arp),
        persisted: true,
      });
      return;
    }
    if (trimmed.startsWith('SAVE_PROFILE,')) {
      const id = profileId(trimmed.split(',')[1]);
      profiles[activeProfile].arp = clone(arp);
      profiles[id] = clone(profiles[activeProfile]);
      emitJson({
        profile_saved: true,
        profile: id,
        active_profile: activeProfile,
      });
      return;
    }
    if (trimmed.startsWith('LOAD_PROFILE,')) {
      const id = profileId(trimmed.split(',')[1]);
      activeProfile = id;
      arp = clone(profiles[id].arp);
      emitJson({
        profile_loaded: true,
        profile: id,
        active_profile: activeProfile,
      });
      return;
    }
    if (trimmed.startsWith('RESET_PROFILE,')) {
      const id = profileId(trimmed.split(',')[1]);
      profiles[id] = clone(defaultProfile);
      if (id === activeProfile) arp = clone(defaultArp);
      emitJson({
        profile_reset: true,
        profile: id,
        active_profile: activeProfile,
      });
      return;
    }
    if (!trimmed.startsWith('SET_ALL ')) return;

    setAllBuffer += trimmed.slice(8);
    try {
      const payload = JSON.parse(setAllBuffer);
      setAllBuffer = '';
      config = clone({
        ...config,
        ...payload.config,
      });
      setTimeout(() => {
        if (!connected) return;
        if (ackMode === 'timeout') return;
        if (ackMode === 'bad-ack') {
          emitJson({
            type: 'ack',
            checksum: 'bad-checksum',
            seq: payload.seq,
          });
          return;
        }
        if (ackMode === 'error') {
          emitJson({
            type: 'error',
            code: 'checksum',
            message: 'simulated apply failure',
            seq: payload.seq,
          });
          return;
        }
        emitJson({
          type: 'ack',
          checksum: payload.checksum,
          seq: payload.seq,
        });
      }, ackDelayMs);
    } catch (_) {
      // wait for the next chunk
    }
  }

  return {
    connect() {
      connected = true;
      events.emit('connect');
    },
    disconnect() {
      connected = false;
      events.emit('disconnect');
    },
    emitMalformed(line = '{"oops"') {
      events.emit('line', line);
    },
    emitTelemetry(payload = { type: 'telemetry', slots: [1, 2, 3] }) {
      emitJson(payload);
    },
    getConfig() {
      return clone(config);
    },
    getManifest() {
      return clone(manifest);
    },
    on(eventName, handler) {
      events.on(eventName, handler);
      return () => events.off(eventName, handler);
    },
    receiveLine,
    setAckDelay(ms) {
      ackDelayMs = Number(ms) || 0;
    },
    setAckMode(mode) {
      ackMode = mode;
    },
    setConfig(nextConfig) {
      config = clone(nextConfig);
    },
  };
}

module.exports = {
  createDefaultConfig,
  createDefaultManifest,
  createDefaultSchema,
  createSimulatedMn42Device,
};
