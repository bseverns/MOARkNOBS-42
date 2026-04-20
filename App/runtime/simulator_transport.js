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
    free_ram: 48000,
    free_flash: 512000
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
      return {
        index: idx,
        type: 'CC',
        type_name: 'CC',
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
          gain: 1
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
      filter: { frequency: 800, q: 1 }
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
      case 'set_config':
        if (request.config && typeof request.config === 'object') {
          config = { ...config, ...request.config };
        }
        respond({ checksum: request.checksum ?? 'sim-checksum' });
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
        config = cloneValue(defaultProfile);
        respond({ slot, config: cloneValue(config) });
        break;
      }
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
