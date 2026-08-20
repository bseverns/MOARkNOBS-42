// Simulator transport used by tests and hardware-free UI rehearsal.
// Keep this mapping in lockstep with the firmware dispatcher. The companion
// protocol guard verifies every declared native command exists in firmware and
// every simulator RPC case is intentionally mapped or marked simulator-only.
export const SIMULATOR_FIRMWARE_COMMANDS = Object.freeze({
  hello: 'HELLO',
  get_manifest: 'GET_MANIFEST',
  get_config: 'GET_CONFIG',
  get_mod_matrix: 'GET_MOD_MATRIX',
  get_profile: 'GET_PROFILE',
  get_clock: 'GET_CLOCK',
  get_arp: 'GET_ARP',
  get_jitter: 'GET_JITTER',
  get_note_dynamics: 'GET_NOTE_DYNAMICS',
  get_usb_midi: 'GET_USB_MIDI',
  set_config: 'SET_ALL',
  set_profile: 'SET_PROFILE',
  set_clock: 'SET_CLOCK',
  set_arp: 'SET_ARP',
  set_jitter: 'SET_JITTER',
  set_note_dynamics: 'SET_NOTE_DYNAMICS',
  set_usb_midi: 'SET_USB_MIDI',
  save_profile: 'SAVE_PROFILE',
  load_profile: 'LOAD_PROFILE',
  reset_profile: 'RESET_PROFILE',
  arp_start: 'ARP_START',
  arp_stop: 'ARP_STOP'
});

export const SIMULATOR_MACRO_COMMANDS = Object.freeze([
  'SAVE_MACRO_SLOT',
  'RECALL_MACRO_SLOT'
]);

export const SIMULATOR_SCENE_COMMANDS = Object.freeze([
  'GET_SCENES',
  'SAVE_SCENE',
  'RECALL_SCENE'
]);

export const SIMULATOR_ONLY_RPCS = Object.freeze([
  'macro_command',
  'scene_command',
  'set_param',
  'hang'
]);

function deterministicUnit(seed, step) {
  let value = (Math.trunc(seed) ^ Math.imul(Math.trunc(step) + 1, 0x9e3779b1)) >>> 0;
  value ^= value >>> 16;
  value = Math.imul(value, 0x7feb352d) >>> 0;
  value ^= value >>> 15;
  value = Math.imul(value, 0x846ca68b) >>> 0;
  value ^= value >>> 16;
  return (value >>> 0) / 0xffffffff;
}

// Deterministic visual rehearsal only. These helpers keep declared shapes and
// musician-facing EF recipes perceptibly distinct without pretending to model
// the analog front end, component tolerances, calibration, or hardware timing.
export function simulateLfoValue(
  config = {},
  sampleIndex = 0,
  { frameMs = 16, seed = 1, bpm = 120 } = {}
) {
  const shape = Math.max(0, Math.min(5, Math.round(Number(config.shape) || 0)));
  const syncTicksPerCycle = [24, 48, 96, 192, 384, 768, 12, 6];
  const syncRatio = Math.max(0, Math.min(7, Math.round(Number(config.sync_ratio) || 0)));
  const frequency = config.sync
    ? (Math.max(20, Math.min(300, Number(bpm) || 120)) / 60 * 24) /
      syncTicksPerCycle[syncRatio]
    : Math.max(0.01, Number(config.frequency_hz) || 1);
  const elapsedSeconds = Math.max(16, Number(frameMs) || 0) * Math.max(0, sampleIndex) / 1000;
  const phase = (elapsedSeconds * frequency) % 1;
  let value;
  if (shape === 0) value = (Math.sin(phase * Math.PI * 2) + 1) / 2;
  else if (shape === 1) value = phase < 0.5 ? phase * 2 : 2 - phase * 2;
  else if (shape === 2) value = phase;
  else if (shape === 3) value = phase < 0.5 ? 1 : 0;
  else {
    const segmentPosition = elapsedSeconds * frequency;
    const segment = Math.floor(segmentPosition);
    const current = deterministicUnit(seed, segment);
    if (shape === 4) value = current;
    else {
      const next = deterministicUnit(seed, segment + 1);
      const mix = segmentPosition - segment;
      const eased = mix * mix * (3 - 2 * mix);
      value = current + (next - current) * eased;
    }
  }
  const depth = Math.max(0, Math.min(1, Number(config.depth) || 0));
  return config.bipolar
    ? Math.max(0, Math.min(1, 0.5 + (value - 0.5) * depth))
    : Math.max(0, Math.min(1, value * depth));
}

export function simulateEfResponse(
  sourceValue,
  settings = {},
  previousValue = 0,
  sampleIndex = 0,
  seed = 1
) {
  const source = Math.max(0, Math.min(127, Number(sourceValue) || 0));
  const mode = Math.max(0, Math.min(3, Math.round(Number(settings.mode) || 0)));
  const filter = String(settings.filter_name ?? 'LINEAR').toUpperCase();
  let target = source;
  if (mode === 2) {
    const threshold = Math.max(0, Math.min(127, Number(settings.gateThreshold) || 16));
    target = source >= threshold ? 127 : 0;
  } else if (filter === 'OPPOSITE_LINEAR') target = 127 - source;
  else if (filter === 'EXPONENTIAL') {
    target = Math.min(127, Math.pow(source / 127, 1.7) * 160);
  } else if (filter === 'RANDOM') {
    const range = Math.max(8, Math.min(48, Number(settings.q) * 12 || 16));
    target = source + (deterministicUnit(seed, sampleIndex) * 2 - 1) * range;
  } else if (filter === 'HIGHPASS') {
    target = 64 + (source - previousValue) * 2;
  } else if (filter === 'BANDPASS') {
    target = 64 + Math.sin((source / 127) * Math.PI * 2) * 50;
  }
  target = Math.max(0, Math.min(127, target));
  let alpha = Math.max(0.02, Math.min(1, Number(settings.smoothing) || 0.2));
  if (filter === 'LOWPASS') alpha = Math.min(alpha, 0.12);
  if (mode === 2) alpha = 1;
  return Math.round(previousValue + (target - previousValue) * alpha);
}

export function createSimulator(simDeps = {}) {
  const {
    createManifest,
    argMethodNames = [],
    efFilterNames = [],
    cloneValue,
    setNested,
    telemetryFrameMs = 16
  } = simDeps;

  if (typeof createManifest !== 'function') {
    throw new Error('createSimulator requires createManifest dependency');
  }
  if (typeof cloneValue !== 'function') {
    throw new Error('createSimulator requires cloneValue dependency');
  }
  if (typeof setNested !== 'function') {
    throw new Error('createSimulator requires setNested dependency');
  }

  let opened = false;
  let index = 0;
  const lines = [];
  let resolver;
  let activeProfile = 0;

  const manifest = {
    ...createManifest(),
    fw_version: 'sim-fw',
    git_sha: 'deadbeef',
    build_time: new Date().toISOString(),
    power_profile: 'POWER_CHOKED_V1',
    led_brightness_cap: 26,
    rail_topology_verified: false,
    display_present: true,
    display_ok: true,
    display_init_failures: 0,
    display_status: 'ok',
    free_ram: 48000,
    free_flash: 512000,
    brownout_count: 2,
    eeprom_primary_valid: true,
    eeprom_backup_valid: true,
    eeprom_last_load: 'primary',
    profile_count: 4,
    active_profile: activeProfile
  };
  const simulatedSlotEfValues = Array.from({ length: manifest.slot_count }, () => 0);
  const slotValues = Array.from(
    { length: manifest.slot_count },
    (_, slotIndex) => (slotIndex * 3) % 128
  );

  let config = {
    fw_version: manifest.fw_version,
    schema_version: manifest.schema_version,
    pots: Array.from({ length: manifest.pot_count }, (_, idx) => ({
      index: idx,
      channel: (idx % 16) + 1,
      cc: (idx * 7) % 128
    })),
    slots: Array.from({ length: manifest.slot_count }, (_, idx) => {
      const efIndex = idx % manifest.envelope_count;
      const filterIndex = idx % efFilterNames.length;
      const argMethod = idx % argMethodNames.length;
      const type = idx === 1 ? 'Note' : 'CC';
      return {
        index: idx,
        type,
        type_name: type,
        channel: (idx % 16) + 1,
        data1: (idx % 120) + 1,
        ef_index: efIndex,
        ef: {
          index: efIndex,
          filter_index: filterIndex,
          filter_name: efFilterNames[filterIndex],
          frequency: 400 + (idx % 8) * 50,
          q: 0.6 + (idx % 5) * 0.1,
          oversample: 4,
          smoothing: 0.2 + (idx % 3) * 0.1,
          baseline: 0,
          gain: 1,
          destination_mode: 'add_clamp',
          destination_mode_name: 'add_clamp'
        },
        ef_payload: {
          type: efFilterNames[filterIndex],
          freq: 400 + (idx % 8) * 50,
          q: 0.6 + (idx % 5) * 0.1
        },
        arg: {
          enabled: idx % 3 === 0,
          method: argMethod,
          method_name: argMethodNames[argMethod],
          sourceA: efIndex,
          sourceB: (efIndex + 1) % manifest.envelope_count
        },
        active: idx % 2 === 0,
        arp_note: (idx * 3) % 128,
        sysexTemplate: ''
      };
    }),
    efSlots: Array.from({ length: manifest.envelope_count }, () => ({ slots: [] })),
    envelopes: {
      routing: Array.from(
        { length: manifest.pot_count },
        (_, idx) => idx % manifest.envelope_count
      ),
      followers: Array.from({ length: manifest.envelope_count }, (_, idx) => ({
        index: idx,
        active: idx % 2 === 0,
        filter: efFilterNames[idx % efFilterNames.length],
        baseline: 0,
        oversample: 4,
        smoothing: 0.25
      })),
      mode: 0,
      mode_name: 'LINEAR',
      arg_method: 0,
      arg_method_name: 'PLUS',
      arg_enable: true,
      arg_pair: { a: 0, b: 1 },
      filter: { frequency: 800, q: 1, idle_floor: 24 },
      idle_floor: 24
    },
    led: {
      brightness: 64,
      hex: '#ff00ff',
      rgb: { r: 255, g: 0, b: 255 }
    }
  };
  let macroSnapshot = null;
  const profileSlots = Array.from({ length: 4 }, () => cloneValue(config));
  const defaultProfile = cloneValue(config);
  const defaultProfileSettings = {
    arp: {
      length_ticks: 12,
      shape: 0,
      swing_percent: 0,
      gate_percent: 50,
      octave_range: 0,
      pattern_length: 4,
      assigned_slots: []
    },
    lfos: [
      {
        index: 0,
        shape: 0,
        frequency_hz: 1,
        depth: 1,
        bipolar: true,
        sync: false,
        sync_ratio: 0
      },
      {
        index: 1,
        shape: 3,
        frequency_hz: 0.5,
        depth: 0.5,
        bipolar: false,
        sync: true,
        sync_ratio: 3
      }
    ],
    routes: [
      {
        type: 4,
        lfo: 0,
        depth: 0.5,
        amount: 100,
        min: 20,
        max: 110,
        target: 6,
        slot: 6,
        channel: 1,
        cc_msb: 74,
        cc_lsb: 32
      },
      {
        type: 2,
        lfo: 1,
        depth: 1,
        amount: -75,
        min: 0,
        max: 127,
        target: 0,
        channel: 1,
        cc_msb: 16,
        cc_lsb: 48
      }
    ]
  };
  const profileSettingsSlots = Array.from({ length: 4 }, () => cloneValue(defaultProfileSettings));
  const activeArpSlots = new Set();
  let liveArp = {
    active: false,
    slot: 0,
    length_ticks: 12,
    shape: 0,
    shape_name: 'up',
    swing_percent: 0,
    gate_percent: 50,
    octave_range: 0,
    pattern_length: 4
  };
  let usbMidiOutEnabled = false;
  let velocityShift = 0;
  let changeProbability = 100;
  let jitterDepth = 1;
  let jitterSmoothness = 0.5;
  let followExternalClock = true;
  let clockOutEnabled = false;
  let tappedBpm = 120;
  let externalBpm = 123.4;
  const lfoShapeNames = ['Sine', 'Triangle', 'Saw', 'Square', 'Sample & Hold', 'Random Slew'];
  const lfoSyncRatioNames = ['1/1', '1/2', '1/4', '1/8', '1/16', '1/32', 'x2', 'x4'];
  const currentLfoConfig = () =>
    profileSettingsSlots[activeProfile].lfos.map((entry, idx) => ({
      index: idx,
      shape: entry.shape,
      shape_name: lfoShapeNames[entry.shape] ?? `Shape ${entry.shape}`,
      frequency_hz: entry.frequency_hz,
      depth: entry.depth,
      bipolar: Boolean(entry.bipolar),
      sync: Boolean(entry.sync),
      sync_ratio: entry.sync_ratio,
      sync_ratio_name: lfoSyncRatioNames[entry.sync_ratio] ?? '-'
    }));

  const nextSlotValues = () => {
    const activeSlot = index % manifest.slot_count;
    slotValues[activeSlot] = (slotValues[activeSlot] + 1) % 128;
    return [...slotValues];
  };

  const telemetry = () => {
    const slots = nextSlotValues();
    // Rehearsal followers model three musically distinct states: broad active
    // phrases, a quiet floor, and periodic threshold crossings. Activity is
    // derived from the same generated level so Scope state explains the trace.
    const envelopeSignals = Array.from({ length: manifest.envelope_count }, (_, efIndex) => {
      const family = efIndex % 3;
      const phase = efIndex * 0.83;
      let sourceValue;
      if (family === 1) {
        sourceValue = Math.round(7 + (Math.sin(index / 9 + phase) + 1) * 3);
      } else if (family === 2) {
        const pulse = Math.max(0, Math.sin(index / (5.2 + efIndex * 0.2) + phase));
        sourceValue = Math.round(10 + Math.pow(pulse, 1.6) * 108);
      } else {
        const phrase = (Math.sin(index / (8 + efIndex * 0.35) + phase) + 1) / 2;
        const ripple = (Math.sin(index / 3.5 + phase * 1.7) + 1) / 2;
        sourceValue = Math.round(10 + Math.pow(phrase, 1.35) * 100 + ripple * 8);
      }
      const activityFloor = family === 1 ? 14 : family === 2 ? 42 : 36;
      // Global EF traces represent the deterministic synthetic physical-source
      // signal only. Slot EF settings intentionally apply later, per slot, so
      // a slot's position in config.slots cannot affect shared Scope telemetry.
      return { sourceValue, value: sourceValue, active: sourceValue >= activityFloor };
    });
    const envelopes = envelopeSignals.map((signal) => signal.value);
    const lfos = profileSettingsSlots[activeProfile].lfos.map((lfo, lfoIndex) =>
      Number(simulateLfoValue(lfo, index, {
        frameMs: telemetryFrameMs * 4,
        seed: lfoIndex + 101,
        bpm: tappedBpm
      }).toFixed(3))
    );
    const efStatus = envelopeSignals.map((signal) => (signal.active ? 1 : 0));
    const slotOutputs = [...slots];
    const slotContributions = [];

    config.slots.forEach((slot, slotIndex) => {
      if (!slot?.active) return;
      let value = slots[slotIndex] ?? 0;
      const baseline = value;
      let efDelta = 0;
      const lfoDeltas = [0, 0];
      let activeMask = 0;
      const efIndex = Number(slot.ef_index ?? slot.ef?.index);
      if (Number.isInteger(efIndex) && envelopeSignals[efIndex]) {
        const before = value;
        const contribution = simulateEfResponse(
          envelopeSignals[efIndex].sourceValue,
          slot.ef ?? {},
          simulatedSlotEfValues[slotIndex] ?? 0,
          index,
          slotIndex + 1001
        );
        simulatedSlotEfValues[slotIndex] = contribution;
        const activityThreshold = Math.max(4, Number(slot.ef?.activityThreshold) || 0);
        if (contribution >= activityThreshold) {
          const mode = String(slot.ef?.destination_mode ?? 'add_clamp').toLowerCase();
          if (mode === 'subtract') value -= contribution;
          else if (mode === 'replace') value = contribution;
          else if (mode === 'scale') value = Math.round((value * contribution) / 127);
          else if (mode === 'centered') value += contribution - 64;
          else value += contribution;
          value = Math.max(0, Math.min(127, value));
          efDelta = value - before;
          activeMask |= 0x01;
        }
      }

      // The simulator's persisted rehearsal profile owns one LFO route to S7.
      if (slotIndex === 6) {
        const before = value;
        value = Math.max(0, Math.min(127, Math.round(64 + (lfos[0] * 2 - 1) * 50)));
        lfoDeltas[0] = value - before;
        activeMask |= 0x02;
      }

      slotOutputs[slotIndex] = value;
      if (activeMask) {
        slotContributions.push({
          index: slotIndex,
          baseline,
          ef: efDelta,
          lfos: lfoDeltas,
          output: value,
          activeMask
        });
      }
    });

    return {
    active_profile: activeProfile,
    slots,
    slotOutputs,
    slotContributions,
    slotArgs: Array.from({ length: manifest.slot_count }, (_, idx) => ({
      enabled: idx % 2 === 0,
      method: idx % argMethodNames.length,
      method_name: argMethodNames[idx % argMethodNames.length],
      sourceA: idx % manifest.envelope_count,
      sourceB: (idx + 1) % manifest.envelope_count
    })),
    envelopes,
    lfos,
    lfo_config: currentLfoConfig(),
    currentSlot: index++ % manifest.slot_count,
    argPair: [0, 1],
    argEnabled: true,
    efStatus,
    diagnostics: {
      loop_max_us: 702,
      loop_last_us: 512,
      midi_isr_max_us: 320,
      midi_isr_last_us: 110,
      midi_drops: 0,
      uart_overruns: 0,
      loop_overruns: 0,
      midi_task_overruns: 0
    },
    note_dynamics: {
      velocity_shift: velocityShift,
      change_probability: changeProbability
    },
    jitter: {
      depth: jitterDepth,
      smoothness: jitterSmoothness
    },
    clock: {
      follow_external: followExternalClock,
      clock_out_enabled: clockOutEnabled,
      tapped_bpm: tappedBpm,
      external_bpm: externalBpm,
      external_signal: true,
      running: true,
      source: followExternalClock ? 'external' : 'internal'
    }
    };
  };

  async function open() {
    opened = true;
  }

  function pushLine(payload) {
    lines.push(JSON.stringify(payload));
    if (resolver) {
      const fn = resolver.resolve;
      resolver = null;
      fn(lines.shift());
    }
  }

  function handleSimulatorMacroCommand(command) {
    if (!command) return false;
    if (command === 'SAVE_MACRO_SLOT') {
      macroSnapshot = cloneValue(config);
      pushLine({ macro_saved: true, macro_available: true });
      return true;
    }
    if (command === 'RECALL_MACRO_SLOT') {
      const hasSnapshot = Boolean(macroSnapshot);
      if (hasSnapshot) {
        config = cloneValue(macroSnapshot);
      }
      pushLine({ macro_recalled: hasSnapshot, macro_available: hasSnapshot });
      return true;
    }
    return false;
  }

  function buildSimulatorModMatrix() {
    const routes = [];
    const ccWriters = new Map();
    const registerCc = (channel, cc, writer) => {
      const key = `${channel}:${cc}`;
      const list = ccWriters.get(key) ?? [];
      list.push(writer);
      ccWriters.set(key, list);
    };
    const slotMidi = (slot = {}) => ({
      type: slot.type_name ?? slot.type ?? 'CC',
      channel: slot.channel ?? slot.midiChannel ?? 1,
      data1: slot.data1 ?? slot.cc ?? 0,
      ...(String(slot.type_name ?? slot.type).toUpperCase() === 'CC'
        ? { cc: slot.data1 ?? slot.cc ?? 0 }
        : {})
    });

    (config.slots ?? []).forEach((slot, index) => {
      if (!slot?.active) return;
      const id = `pot${index}`;
      const midi = slotMidi(slot);
      routes.push({
        id,
        source: id,
        source_type: 'pot',
        transform: 'direct 0-127',
        destination: `slot${index}.value`,
        mode: 'replace',
        exit: 'midi',
        active: true,
        persisted: true,
        range: { min: 0, max: 127 },
        midi
      });
      if (midi.type === 'CC' && Number.isFinite(Number(midi.cc)))
        registerCc(midi.channel, midi.cc, id);

      const efIndex = Number(slot.ef_index ?? slot.ef?.index);
      if (Number.isFinite(efIndex) && efIndex >= 0) {
        const efId = `ef${efIndex}_slot${index}`;
        const cc = config.pots?.[index]?.cc ?? midi.cc ?? 0;
        const channel = config.pots?.[index]?.channel ?? midi.channel ?? 1;
        routes.push({
          id: efId,
          source: `ef${efIndex}`,
          source_type: 'ef',
          transform: `${slot.ef?.filter_name ?? 'Linear'} gain ${slot.ef?.gain ?? 1}`,
          destination: `slot${index}.value`,
          mode: slot.ef?.destination_mode ?? 'add_clamp',
          exit: 'midi_cc',
          active: Boolean(slot.active),
          persisted: true,
          amount: 1,
          range: { min: 0, max: 127 },
          midi: { type: 'CC', channel, cc }
        });
        registerCc(channel, cc, efId);
      }
      if (slot.arg?.enabled) {
        routes.push({
          id: `arg_slot${index}`,
          source: `ef${slot.arg.sourceA}+ef${slot.arg.sourceB}`,
          source_type: 'arg',
          transform: slot.arg.method_name ?? 'PLUS',
          destination: `slot${index}.value`,
          mode: 'pre_add_arg',
          exit: 'internal_to_ef',
          active: Boolean(slot.active),
          persisted: true,
          range: { min: 0, max: 127 }
        });
      }
    });

    (profileSettingsSlots[0].routes ?? []).forEach((route, index) => {
      const source = `lfo${route.lfo ?? 0}`;
      const id = `${source}_route${index}`;
      const slot = config.slots?.[route.slot ?? route.target];
      const midi = slot ? slotMidi(slot) : null;
      const matrixRoute = {
        id,
        source,
        source_type: 'lfo',
        route_type:
          ['internal', 'midi_cc7', 'midi_cc14', 'osc', 'slot_value'][route.type] ?? 'unknown',
        transform: `sim depth ${route.depth ?? 1}`,
        destination: route.type === 4 ? `slot${route.slot ?? route.target}.value` : 'midi.cc14',
        mode: 'replace',
        exit: route.type === 4 ? 'midi' : 'midi_cc14',
        depth: route.depth ?? 1,
        amount: route.amount ?? 100,
        rateLimitMs: 9,
        active: true,
        persisted: true,
        last_value: 64,
        range: { min: route.min ?? 0, max: route.max ?? 127 }
      };
      if (midi) matrixRoute.midi = midi;
      if (route.type === 2) {
        matrixRoute.channel = route.channel ?? 1;
        matrixRoute.cc_msb = route.cc_msb ?? 0;
        matrixRoute.cc_lsb = route.cc_lsb ?? 32;
        registerCc(matrixRoute.channel, matrixRoute.cc_msb, id);
        registerCc(matrixRoute.channel, matrixRoute.cc_lsb, id);
      } else if (midi?.type === 'CC' && Number.isFinite(Number(midi.cc))) {
        registerCc(midi.channel, midi.cc, id);
      }
      routes.push(matrixRoute);
    });

    const conflicts = Array.from(ccWriters.entries())
      .filter(([, writers]) => writers.length > 1)
      .map(([key, writers]) => {
        const [channel, cc] = key.split(':').map(Number);
        return {
          target: 'midi.cc',
          channel,
          cc,
          writers: writers.join(', '),
          message: `${writers.length} live modulators write CC ${cc} on channel ${channel}`
        };
      });

    return {
      command: 'GET_MOD_MATRIX',
      contract_version: 1,
      sources: {
        ef: Array.from({ length: manifest.envelope_count }, (_, idx) => idx),
        lfo: Array.from({ length: manifest.lfo_count ?? 2 }, (_, idx) => idx),
        pot: Array.from({ length: manifest.slot_count }, (_, idx) => idx)
      },
      routes,
      conflicts
    };
  }

  async function writeLine(line) {
    if (!opened) throw new Error('simulator closed');
    const trimmed = line.trim();
    if (!trimmed) return;
    if (handleSimulatorMacroCommand(trimmed)) return;
    let request;
    try {
      request = JSON.parse(trimmed);
    } catch (err) {
      pushLine({ error: { message: err.message } });
      return;
    }
    const rpc = request.rpc ?? request.method;
    const respond = (result) => {
      if (request.id === undefined) return;
      pushLine({ id: request.id, result });
    };
    const clampSlot = (value) => {
      const idx = Number.isFinite(Number(value)) ? Number(value) : 0;
      return Math.max(0, Math.min(profileSlots.length - 1, Math.floor(idx)));
    };
    switch (rpc) {
      case 'macro_command': {
        const command = String(request.command ?? '');
        if (command === 'SAVE_MACRO_SLOT') {
          macroSnapshot = cloneValue(config);
          respond({ macro_saved: true, macro_available: true });
        } else if (command === 'RECALL_MACRO_SLOT') {
          const hasSnapshot = Boolean(macroSnapshot);
          if (hasSnapshot) config = cloneValue(macroSnapshot);
          respond({ macro_recalled: hasSnapshot, macro_available: hasSnapshot });
        } else {
          respond({ error: 'Unknown macro command' });
        }
        break;
      }
      case 'scene_command': {
        const command = request.payload?.cmd;
        if (command === 'GET_SCENES') respond({ scenes: [] });
        else if (command === 'SAVE_SCENE') respond({ scene_saved: true, scene_slot: request.payload?.slot ?? 0 });
        else if (command === 'RECALL_SCENE') respond({ scene_recalled: true, scene_slot: request.payload?.slot ?? 0 });
        else respond({ scene_error: 'Unknown scene command' });
        break;
      }
      case 'hello':
        respond({ message: 'hello' });
        break;
      case 'get_manifest':
        manifest.active_profile = activeProfile;
        respond({ manifest });
        break;
      case 'get_config':
        respond({ config });
        break;
      case 'get_mod_matrix':
        respond(buildSimulatorModMatrix());
        break;
      case 'get_profile': {
        const slot = clampSlot(request.slot ?? request.id ?? 0);
        respond({
          profile: slot,
          active_profile: activeProfile,
          active: slot === activeProfile,
          stored: true,
          arp: cloneValue(profileSettingsSlots[slot].arp),
          lfos: cloneValue(profileSettingsSlots[slot].lfos),
          routes: cloneValue(profileSettingsSlots[slot].routes),
          slots: []
        });
        break;
      }
      case 'get_clock':
        respond({
          follow_external: followExternalClock,
          clock_out_enabled: clockOutEnabled,
          tapped_bpm: tappedBpm,
          external_bpm: externalBpm,
          external_signal: true,
          running: true,
          source: followExternalClock ? 'external' : 'internal'
        });
        break;
      case 'get_arp':
        respond({ ...liveArp, active: activeArpSlots.size > 0 });
        break;
      case 'get_jitter':
        respond({ depth: jitterDepth, smoothness: jitterSmoothness });
        break;
      case 'get_note_dynamics':
        respond({ velocity_shift: velocityShift, change_probability: changeProbability });
        break;
      case 'get_usb_midi':
        respond({ usb_midi_out: usbMidiOutEnabled });
        break;
      case 'set_config':
        if (request.config && typeof request.config === 'object') {
          config = { ...config, ...request.config };
        }
        respond({ checksum: request.checksum ?? 'sim-checksum' });
        break;
      case 'set_profile': {
        const slot = clampSlot(request.slot ?? request.id ?? 0);
        const next = request.profile && typeof request.profile === 'object' ? request.profile : {};
        if (next.arp && typeof next.arp === 'object') {
          profileSettingsSlots[slot].arp = {
            ...profileSettingsSlots[slot].arp,
            ...cloneValue(next.arp)
          };
        }
        if (Array.isArray(next.lfos)) {
          profileSettingsSlots[slot].lfos = cloneValue(next.lfos);
        }
        if (Array.isArray(next.routes)) {
          profileSettingsSlots[slot].routes = cloneValue(next.routes);
        }
        respond({
          profile: slot,
          active_profile: activeProfile,
          profile_set: true,
          active_applied: slot === activeProfile
        });
        break;
      }
      case 'set_clock':
        followExternalClock = Boolean(request.followExternal);
        clockOutEnabled = Boolean(request.clockOutEnabled);
        tappedBpm = Math.max(20, Math.min(300, Number(request.tappedBpm) || 120));
        respond({
          command: 'SET_CLOCK',
          status: 'ok',
          follow_external: followExternalClock,
          clock_out_enabled: clockOutEnabled,
          tapped_bpm: tappedBpm,
          external_bpm: externalBpm,
          external_signal: true,
          running: true,
          source: followExternalClock ? 'external' : 'internal'
        });
        break;
      case 'set_arp': {
        const shapeNames = ['up', 'down', 'up_down', 'random', 'drunk', 'euclidean'];
        liveArp = {
          ...liveArp,
          active: activeArpSlots.size > 0,
          length_ticks: Math.max(1, Math.min(24, Math.round(Number(request.lengthTicks) || 12))),
          shape: Math.max(0, Math.min(5, Math.round(Number(request.shape) || 0))),
          swing_percent: Math.max(0, Math.min(80, Math.round(Number(request.swingPercent) || 0))),
          gate_percent: Math.max(5, Math.min(100, Math.round(Number(request.gatePercent) || 50))),
          octave_range: Math.max(0, Math.min(3, Math.round(Number(request.octaveRange) || 0))),
          pattern_length:
            Number.isFinite(Number(request.patternLength)) &&
            Number(request.patternLength) >= 2 &&
            Number(request.patternLength) <= 16
              ? Math.round(Number(request.patternLength))
              : liveArp.pattern_length
        };
        liveArp.shape_name = shapeNames[liveArp.shape] ?? 'up';
        respond({ command: 'SET_ARP', status: 'ok', ...liveArp });
        break;
      }
      case 'set_jitter':
        jitterDepth = Math.max(0, Math.min(1, Number(request.depth) || 0));
        jitterSmoothness = Math.max(0, Math.min(1, Number(request.smoothness) || 0));
        respond({
          command: 'SET_JITTER',
          status: 'ok',
          depth: jitterDepth,
          smoothness: jitterSmoothness
        });
        break;
      case 'set_note_dynamics':
        velocityShift = Math.max(-64, Math.min(63, Math.round(Number(request.velocityShift) || 0)));
        changeProbability = Math.max(
          0,
          Math.min(100, Math.round(Number(request.changeProbability) || 0))
        );
        respond({
          command: 'SET_NOTE_DYNAMICS',
          status: 'ok',
          velocity_shift: velocityShift,
          change_probability: changeProbability
        });
        break;
      case 'set_usb_midi':
        usbMidiOutEnabled = Boolean(request.enabled);
        respond({ command: 'SET_USB_MIDI', status: 'ok', usb_midi_out: usbMidiOutEnabled });
        break;
      case 'set_param':
        if (typeof request.path !== 'string' || !request.path.length) {
          if (request.id !== undefined) {
            pushLine({ id: request.id, error: { message: 'set_param requires path' } });
          }
          break;
        }
        setNested(config, request.path, request.value);
        respond({ ok: true, path: request.path, value: request.value });
        break;
      case 'save_profile': {
        const slot = clampSlot(request.slot ?? request.id ?? 0);
        activeProfile = slot;
        manifest.active_profile = activeProfile;
        profileSlots[slot] = cloneValue(config);
        respond({ slot, active_profile: activeProfile, saved: true });
        break;
      }
      case 'load_profile': {
        const slot = clampSlot(request.slot ?? request.id ?? 0);
        activeProfile = slot;
        manifest.active_profile = activeProfile;
        const loaded = profileSlots[slot] ?? cloneValue(defaultProfile);
        config = cloneValue(loaded);
        respond({ slot, active_profile: activeProfile, config: cloneValue(config) });
        break;
      }
      case 'reset_profile': {
        const slot = clampSlot(request.slot ?? request.id ?? 0);
        activeProfile = slot;
        manifest.active_profile = activeProfile;
        profileSlots[slot] = cloneValue(defaultProfile);
        profileSettingsSlots[slot] = cloneValue(defaultProfileSettings);
        config = cloneValue(defaultProfile);
        respond({ slot, active_profile: activeProfile, config: cloneValue(config) });
        break;
      }
      case 'arp_start':
        {
          const slot = Math.max(
            0,
            Math.min(manifest.slot_count - 1, Math.floor(Number(request.slot) || 0))
          );
          activeArpSlots.add(slot);
          liveArp = { ...liveArp, active: true, slot };
          respond({ slot, arp_started: true, active: activeArpSlots.size > 0 });
        }
        break;
      case 'arp_stop':
        if (request.slot === undefined || request.slot === null) {
          activeArpSlots.clear();
          liveArp = { ...liveArp, active: false };
          respond({ arp_stopped: true, active: false });
          break;
        }
        {
          const slot = Math.max(
            0,
            Math.min(manifest.slot_count - 1, Math.floor(Number(request.slot) || 0))
          );
          activeArpSlots.delete(slot);
          liveArp = { ...liveArp, active: activeArpSlots.size > 0 };
          respond({ slot, arp_stopped: true, active: activeArpSlots.size > 0 });
        }
        break;
      case 'hang':
        break;
      default:
        if (request.id !== undefined) {
          pushLine({ id: request.id, error: { message: 'Unsupported RPC' } });
        }
        break;
    }
  }

  function nextLine() {
    if (!opened) return Promise.reject(new Error('simulator closed'));
    if (lines.length) return Promise.resolve(lines.shift());
    return new Promise((resolve, reject) => {
      resolver = { resolve, reject };
      setTimeout(() => {
        if (!resolver) return;
        pushLine({ type: 'telemetry', ...telemetry() });
      }, telemetryFrameMs * 4);
    });
  }

  async function close() {
    opened = false;
    if (resolver) {
      resolver.reject(new Error('simulator closed'));
      resolver = null;
    }
  }

  return {
    open,
    writeLine,
    nextLine,
    close,
    rawPort: { getInfo: () => ({ usbVendorId: 0xfeed, usbProductId: 0xbeef }) },
    protocol: 'json-rpc'
  };
}
