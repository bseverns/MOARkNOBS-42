// Simulator transport used by tests and hardware-free UI rehearsal.
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

  const manifest = {
    ...createManifest(),
    fw_version: 'sim-fw',
    git_sha: 'deadbeef',
    build_time: new Date().toISOString(),
    power_profile: 'POWER_CHOKED_V1',
    led_brightness_cap: 26,
    rail_topology_verified: false,
    free_ram: 48000,
    free_flash: 512000,
    brownout_count: 2,
    eeprom_primary_valid: true,
    eeprom_backup_valid: true,
    eeprom_last_load: 'primary'
  };

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
      octave_range: 0
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

  const telemetry = () => ({
    slots: Array.from({ length: manifest.slot_count }, () => Math.floor(Math.random() * 127)),
    slotArgs: Array.from({ length: manifest.slot_count }, (_, idx) => ({
      enabled: idx % 2 === 0,
      method: idx % argMethodNames.length,
      method_name: argMethodNames[idx % argMethodNames.length],
      sourceA: idx % manifest.envelope_count,
      sourceB: (idx + 1) % manifest.envelope_count
    })),
    envelopes: Array.from({ length: manifest.envelope_count }, () =>
      Math.floor(Math.random() * 127)
    ),
    lfos: [((index % 40) / 39).toFixed(3), (((index + 20) % 40) / 39).toFixed(3)].map(Number),
    currentSlot: index++ % manifest.slot_count,
    argPair: [0, 1],
    argEnabled: true,
    efStatus: Array.from({ length: manifest.envelope_count }, (_, idx) => (idx % 2 === 0 ? 1 : 0)),
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
  });

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
      case 'hello':
        respond({ message: 'hello' });
        break;
      case 'get_manifest':
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
        respond({ profile: slot, profile_set: true });
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
          octave_range: Math.max(0, Math.min(3, Math.round(Number(request.octaveRange) || 0)))
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
        profileSlots[slot] = cloneValue(config);
        respond({ slot, saved: true });
        break;
      }
      case 'load_profile': {
        const slot = clampSlot(request.slot ?? request.id ?? 0);
        const loaded = profileSlots[slot] ?? cloneValue(defaultProfile);
        config = cloneValue(loaded);
        respond({ slot, config: cloneValue(config) });
        break;
      }
      case 'reset_profile': {
        const slot = clampSlot(request.slot ?? request.id ?? 0);
        profileSlots[slot] = cloneValue(defaultProfile);
        profileSettingsSlots[slot] = cloneValue(defaultProfileSettings);
        config = cloneValue(defaultProfile);
        respond({ slot, config: cloneValue(config) });
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
