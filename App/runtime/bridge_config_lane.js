function preflightError(message) {
  const error = new Error(message);
  error.bridgeFailureClass = 'preflight-rejected';
  return error;
}

export function createBridgeConfigLane({ getBridgeSessionRuntime }) {
  function requireClient() {
    const client = getBridgeSessionRuntime()?.ensureClient();
    if (!client) throw preflightError('Bridge session unavailable');
    return client;
  }

  return {
    async stageConfig(config) {
      return requireClient().stageConfig(config);
    },

    async applyConfig({ candidate, identity } = {}) {
      const runtime = getBridgeSessionRuntime();
      const client = requireClient();
      let stageReceipt;
      try {
        stageReceipt = await client.stageConfig(candidate, {
          expectedSessionRevision: runtime?.getSessionRevision(),
          ...identity
        });
      } catch (err) {
        // No serial Apply may begin until the identity-bearing stage succeeds.
        err.bridgeFailureClass = 'preflight-rejected';
        runtime?.recordSessionRevision(err.bridgeSession?.sessionRevision);
        throw err;
      }
      runtime?.recordStageReceipt(stageReceipt);

      let response;
      try {
        response = await client.applyConfig({
          expectedSessionRevision: runtime?.getSessionRevision(),
          ...identity
        });
      } catch (err) {
        if (err.bridgeFailureClass === 'preflight-rejected') {
          runtime?.recordSessionRevision(err.bridgeSession?.sessionRevision);
        }
        throw err;
      }

      const result = response?.result;
      if (result?.applied !== true && !(result?.applied === false && result?.reason === 'clean')) {
        const error = new Error('Bridge Apply response omitted its transaction result.');
        error.code = 'invalid_bridge_apply_response';
        error.bridgeFailureClass = 'transmission-unknown';
        error.bridgeSession = response?.session ?? null;
        throw error;
      }
      if (response?.session) runtime?.applyAuthoritativeSession(response.session);
      return {
        ...result,
        authoritativeConfig: response?.session?.liveConfig ?? null
      };
    },

    async refreshSession() {
      return getBridgeSessionRuntime()?.refreshSessionSnapshot({
        warm: false,
        emitConnectedConfig: false
      });
    },

    async rollbackConfig(reason) {
      const client = getBridgeSessionRuntime()?.ensureClient();
      if (!client) return { rolledBack: false };
      return client.rollbackConfig(reason);
    }
  };
}

