const { strict: assert } = require('node:assert');

// Exercise the same on-disk CommonJS staging path used by pkg executables.
// This catches new App runtime dependencies and ESM syntax before packaging.
process.pkg = {};

const {
  PACKAGED_RUNTIME_ROOT,
  loadSchemaAuthority,
} = require('../lib/device/schema_authority');

async function run() {
  const authority = await loadSchemaAuthority();

  assert.equal(authority.schema?.type, 'object');
  assert.equal(authority.configIdentityVersion, 1);
  assert.equal(typeof authority.normalizeConfig, 'function');
  assert.equal(typeof authority.configDigest, 'function');
  assert.equal(authority.appRoot.startsWith(PACKAGED_RUNTIME_ROOT), true);

  console.log('packaged schema authority stages all App runtime dependencies');
}

run().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
