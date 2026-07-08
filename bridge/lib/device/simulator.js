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
    schema_version: 6,
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
      macro_snapshot: true,
      scenes: true,
    },
    ...overrides,
  };
}

function createDefaultSchema(overrides = {}) {
  return {
    type: 'object',
    schema_version: 6,
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
  let connected = true;

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
