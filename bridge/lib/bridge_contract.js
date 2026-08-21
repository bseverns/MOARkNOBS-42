const { version: bridgeVersion } = require('../package.json');
const { MN42_MANIFEST_CONTRACT } = require('./manifest_contract');

const BRIDGE_API_VERSION = 1;
const EVENT_CONTRACT_VERSION = 1;

function normalizeSourceSha(value) {
  const candidate = typeof value === 'string' ? value.trim() : '';
  return candidate || null;
}

function createBridgeContract({
  sourceSha = process.env.MN42_BRIDGE_SOURCE_SHA ?? process.env.GITHUB_SHA,
} = {}) {
  return Object.freeze({
    bridge_api_version: BRIDGE_API_VERSION,
    event_contract_version: EVENT_CONTRACT_VERSION,
    bridge_version: bridgeVersion,
    bridge_source_sha: normalizeSourceSha(sourceSha),
    supported_schema_versions: Object.freeze([
      MN42_MANIFEST_CONTRACT.schema_version,
    ]),
    verified_apply: true,
    structured_session: true,
  });
}

module.exports = {
  BRIDGE_API_VERSION,
  EVENT_CONTRACT_VERSION,
  createBridgeContract,
};
