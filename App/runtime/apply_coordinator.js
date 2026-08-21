import { assertApplyAllowed } from './contract_policy.js';

export function createApplyCoordinator({
  configSession,
  bridgeSessionRuntime,
  isBridgeSessionActive,
  getContractQuality
}) {
  return {
    async apply() {
      assertApplyAllowed(getContractQuality());
      if (!isBridgeSessionActive()) return configSession.apply();
      if (!bridgeSessionRuntime.isHealthy()) {
        throw new Error('Apply is blocked while the Bridge session event authority is stale.');
      }
      await bridgeSessionRuntime.flushStageSync({ active: true });
      bridgeSessionRuntime.suspendStageSync();
      try {
        return await configSession.apply();
      } finally {
        bridgeSessionRuntime.resumeStageSync({ active: true });
      }
    },

    async rollback() {
      bridgeSessionRuntime.cancelStageSync();
      return configSession.rollback();
    },

    async resynchronize() {
      return configSession.resynchronize();
    }
  };
}

