export function createLiveControlsRuntime({
  emit,
  sendRpc,
  getTransport,
  configSession,
  setNestedValue,
  ensureConfigBootTransport,
  disconnect,
  getUseSimulator,
  macroResponseKeys,
  macroCommandTimeoutMs,
  sceneResponseKeys,
  sceneCommandTimeoutMs
} = {}) {
  let macroPending = null;
  let macroAvailability = true;
  let scenePending = null;
  const potGuard = new Set();

  function settleMacroPending(pending, { error, response } = {}) {
    if (!pending) return;
    if (pending.timer) {
      clearTimeout(pending.timer);
      pending.timer = null;
    }
    if (macroPending === pending) {
      macroPending = null;
    }
    if (error) {
      pending.reject(error);
    } else {
      pending.resolve(response);
    }
  }

  function settleScenePending(pending, { error, response } = {}) {
    if (!pending) return;
    if (pending.timer) {
      clearTimeout(pending.timer);
      pending.timer = null;
    }
    if (scenePending === pending) {
      scenePending = null;
    }
    if (error) {
      pending.reject(error);
      return;
    }
    pending.resolve(response);
  }

  function onFatalError(error) {
    settleMacroPending(macroPending, { error });
    settleScenePending(scenePending, { error });
  }

  function sendMacroCommand(command, { timeoutMs } = {}) {
    if (!command || !macroResponseKeys[command]) {
      return Promise.reject(new Error('Unknown macro command'));
    }
    const transport = getTransport();
    if (!transport) return Promise.reject(new Error('Not connected'));
    if (macroPending) return Promise.reject(new Error('Macro command already in progress'));
    let resolveFn;
    let rejectFn;
    const promise = new Promise((resolve, reject) => {
      resolveFn = resolve;
      rejectFn = reject;
    });
    const pending = { command, resolve: resolveFn, reject: rejectFn, timer: null };
    macroPending = pending;
    const timeout = Number.isFinite(Number(timeoutMs)) ? Number(timeoutMs) : macroCommandTimeoutMs;
    pending.timer = setTimeout(() => {
      settleMacroPending(pending, { error: new Error('Macro command timed out') });
    }, timeout);
    transport.writeLine(command).catch((err) => {
      settleMacroPending(pending, { error: err });
    });
    return promise;
  }

  function handleMacroLine(msg) {
    if (!msg || typeof msg !== 'object') return false;
    const hasMacroField =
      Object.prototype.hasOwnProperty.call(msg, 'macro_saved') ||
      Object.prototype.hasOwnProperty.call(msg, 'macro_recalled') ||
      Object.prototype.hasOwnProperty.call(msg, 'macro_available');
    if (!hasMacroField) return false;
    if (Object.prototype.hasOwnProperty.call(msg, 'macro_available')) {
      macroAvailability = Boolean(msg.macro_available);
    }
    emit('macro', {
      saved: Boolean(msg.macro_saved),
      recalled: Boolean(msg.macro_recalled),
      available: macroAvailability,
      raw: msg
    });
    const pending = macroPending;
    if (pending) {
      const expectedKey = macroResponseKeys[pending.command];
      if (expectedKey && Object.prototype.hasOwnProperty.call(msg, expectedKey)) {
        const success = Boolean(msg[expectedKey]);
        if (success) {
          settleMacroPending(pending, { response: msg });
        } else {
          const errorMessage = msg.error?.message ?? `${pending.command} failed`;
          settleMacroPending(pending, { error: new Error(errorMessage) });
        }
      }
    }
    return true;
  }

  function sendSceneCommand(payload, { timeoutMs } = {}) {
    if (!payload || typeof payload !== 'object' || typeof payload.cmd !== 'string') {
      return Promise.reject(new Error('Scene command requires cmd property'));
    }
    const transport = getTransport();
    if (!transport) return Promise.reject(new Error('Not connected'));
    if (scenePending) return Promise.reject(new Error('Scene command already in progress'));
    const expectedKey = sceneResponseKeys[payload.cmd];
    if (!expectedKey) return Promise.reject(new Error('Unknown scene command'));
    let resolveFn;
    let rejectFn;
    const promise = new Promise((resolve, reject) => {
      resolveFn = resolve;
      rejectFn = reject;
    });
    const pending = {
      command: payload.cmd,
      expectedKey,
      resolve: resolveFn,
      reject: rejectFn,
      timer: null
    };
    scenePending = pending;
    const timeout = Number.isFinite(Number(timeoutMs)) ? Number(timeoutMs) : sceneCommandTimeoutMs;
    pending.timer = setTimeout(() => {
      settleScenePending(pending, { error: new Error('Scene command timed out') });
    }, timeout);
    transport.writeLine(JSON.stringify(payload)).catch((err) => {
      settleScenePending(pending, { error: err });
    });
    return promise;
  }

  function handleSceneLine(msg) {
    if (!msg || typeof msg !== 'object') return false;
    const hasSceneField =
      Object.prototype.hasOwnProperty.call(msg, 'scene_saved') ||
      Object.prototype.hasOwnProperty.call(msg, 'scene_recalled') ||
      Array.isArray(msg.scenes);
    if (!hasSceneField) return false;

    if (Array.isArray(msg.scenes)) {
      const scenes = msg.scenes.map((entry) => ({
        slot: Number.isFinite(Number(entry?.slot)) ? Number(entry.slot) : 0,
        name: typeof entry?.name === 'string' ? entry.name : '',
        available: Boolean(entry?.available)
      }));
      emit('scene', { type: 'list', scenes });
    }

    const pending = scenePending;
    if (pending) {
      const expectedKey = pending.expectedKey;
      if (Object.prototype.hasOwnProperty.call(msg, expectedKey)) {
        const success = expectedKey === 'scenes' ? true : Boolean(msg[expectedKey]);
        if (success) {
          settleScenePending(pending, { response: msg });
        } else {
          const errorMessage =
            msg.scene_error?.message ?? msg.scene_error ?? `${pending.command} failed`;
          settleScenePending(pending, { error: new Error(errorMessage) });
        }
      }
    }

    if (Object.prototype.hasOwnProperty.call(msg, 'scene_saved')) {
      emit('scene', {
        type: 'saved',
        slot: Number.isFinite(Number(msg.scene_slot)) ? Number(msg.scene_slot) : -1,
        name: typeof msg.scene_name === 'string' ? msg.scene_name : '',
        available: Boolean(msg.scene_available),
        raw: msg
      });
    }
    if (Object.prototype.hasOwnProperty.call(msg, 'scene_recalled')) {
      emit('scene', {
        type: 'recalled',
        slot: Number.isFinite(Number(msg.scene_slot)) ? Number(msg.scene_slot) : -1,
        name: typeof msg.scene_name === 'string' ? msg.scene_name : '',
        available: Boolean(msg.scene_available),
        raw: msg
      });
    }

    return true;
  }

  function requestScenes(options = {}) {
    return sendSceneCommand({ cmd: 'GET_SCENES' }, options);
  }

  function applyPatch(path, value) {
    if (!path || typeof path !== 'string') {
      return Promise.reject(new Error('Invalid patch path'));
    }
    configSession.stage((draft) => {
      setNestedValue(draft, path, value);
      return draft;
    });
    return sendRpc({ rpc: 'set_param', path, value });
  }

  async function requestConfiguratorBoot() {
    if (getUseSimulator()) {
      emit('status', {
        stage: 'config-boot',
        level: 'warn',
        message: 'Config boot is only available on a physical Web Serial device.'
      });
      return { requested: false, simulator: true };
    }

    await ensureConfigBootTransport();

    try {
      emit('status', {
        stage: 'config-boot',
        level: 'info',
        message: 'Requesting one-shot configurator boot…'
      });
      await sendRpc({ rpc: 'enter_config_mode' }, { timeoutMs: 1500 });
      emit('status', {
        stage: 'config-boot',
        level: 'ok',
        message: 'Device is rebooting into configurator mode. Reconnect after USB reappears.'
      });
      return { requested: true };
    } finally {
      await disconnect();
    }
  }

  function setPotGuard(indices, enabled) {
    indices.forEach((idx) => {
      if (enabled) potGuard.add(idx);
      else potGuard.delete(idx);
    });
    emit('pot-guard', { guards: new Set(potGuard) });
  }

  return {
    onFatalError,
    sendMacroCommand,
    handleMacroLine,
    sendSceneCommand,
    handleSceneLine,
    requestScenes,
    applyPatch,
    requestConfiguratorBoot,
    setPotGuard
  };
}
