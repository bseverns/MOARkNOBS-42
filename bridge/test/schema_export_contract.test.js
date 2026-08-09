const { strict: assert } = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');

const { loadSchemaAuthority } = require('../lib/device/schema_authority');
const {
  createDefaultConfig,
  createDefaultManifest,
} = require('../lib/device/simulator');

function functionBody(source, name) {
  const marker = `void ${name}(`;
  const start = source.indexOf(marker);
  assert.notEqual(start, -1, `missing firmware export writer ${name}`);
  const open = source.indexOf('{', start);
  let depth = 0;
  for (let index = open; index < source.length; index += 1) {
    if (source[index] === '{') depth += 1;
    if (source[index] === '}') depth -= 1;
    if (depth === 0) return source.slice(open + 1, index);
  }
  throw new Error(`unterminated firmware export writer ${name}`);
}

function assignedKeys(source, name, objectName) {
  const body = functionBody(source, name);
  const assignmentPattern = new RegExp(`\\b${objectName}\\["([^"]+)"\\]\\s*=`, 'g');
  const nestedPattern = new RegExp(
    `\\b${objectName}\\.createNested(?:Object|Array)\\("([^"]+)"\\)`,
    'g',
  );
  return [
    ...body.matchAll(assignmentPattern),
    ...body.matchAll(nestedPattern),
  ].map((match) => match[1]).sort();
}

function assertWriterKeys(source, name, objectName, value) {
  assert.deepEqual(
    Object.keys(value).sort(),
    assignedKeys(source, name, objectName),
    `${name} and the schema-contract export fixture drifted`,
  );
}

function assertCombinedWriterKeys(source, writers, value, label) {
  const keys = writers.flatMap(([name, objectName]) => assignedKeys(source, name, objectName));
  assert.deepEqual(
    Object.keys(value).sort(),
    [...new Set(keys)].sort(),
    `${label} and the schema-contract export fixture drifted`,
  );
}

async function run() {
  const root = path.resolve(__dirname, '..', '..');
  const firmwareSource = fs.readFileSync(
    path.join(root, 'firmware', 'src', 'protocol', 'ProtocolSimpleHandlers.cpp'),
    'utf8',
  );
  const manifest = createDefaultManifest();
  const exported = createDefaultConfig(manifest);

  assertWriterKeys(firmwareSource, 'handleGetConfigCommand', 'doc', exported);
  assertWriterKeys(firmwareSource, 'writePotMappings', 'pot', exported.pots[0]);
  assertWriterKeys(firmwareSource, 'writeSlotConfig', 'slotObj', exported.slots[0]);
  assertWriterKeys(firmwareSource, 'writeSlotEfConfig', 'ef', exported.slots[0].ef);
  assertWriterKeys(
    firmwareSource,
    'writeSlotEnvelopePayload',
    'efPayload',
    exported.slots[0].ef_payload,
  );
  assertWriterKeys(firmwareSource, 'writeSlotArgConfig', 'argObj', exported.slots[0].arg);
  assertWriterKeys(firmwareSource, 'writeSlotLfoConfig', 'laneObj', exported.slots[0].lfo[0]);
  assertWriterKeys(firmwareSource, 'writeEfSlotMappings', 'mapping', exported.efSlots[0]);
  assertCombinedWriterKeys(
    firmwareSource,
    [
      ['writeEnvelopeRuntime', 'env'],
      ['writeEnvelopeFilterViews', 'env'],
    ],
    exported.envelopes,
    'envelope export',
  );
  assertWriterKeys(
    firmwareSource,
    'writeEnvelopeRuntime',
    'follower',
    exported.envelopes.followers[0],
  );
  assertWriterKeys(
    firmwareSource,
    'writeEnvelopeRuntime',
    'argPair',
    exported.envelopes.arg_pair,
  );
  assertWriterKeys(
    firmwareSource,
    'writeEnvelopeFilterViews',
    'envFilter',
    exported.envelopes.filter,
  );
  assertWriterKeys(firmwareSource, 'writeEnvelopeFilterViews', 'rootFilter', exported.filter);
  assertWriterKeys(firmwareSource, 'writeRootArgConfig', 'rootArg', exported.arg);
  assertWriterKeys(firmwareSource, 'writeLedConfig', 'led', exported.led);
  assertWriterKeys(firmwareSource, 'writeLedConfig', 'colorObj', exported.led.rgb);

  const authority = await loadSchemaAuthority();
  const normalized = authority.normalizeConfig(exported, manifest);
  const initialValidation = authority.validateConfig(normalized);
  assert.equal(
    initialValidation.valid,
    true,
    `firmware-shaped export failed schema validation: ${JSON.stringify(initialValidation.errors)}`,
  );

  const compact = authority.compactConfigForDevice(normalized, null, {
    clone: (value) => JSON.parse(JSON.stringify(value)),
    slotTypeNames: authority.slotTypeNames,
  });
  const readback = authority.normalizeConfig(compact, manifest);
  const readbackValidation = authority.validateConfig(readback);
  assert.equal(
    readbackValidation.valid,
    true,
    `serialized readback failed schema validation: ${JSON.stringify(readbackValidation.errors)}`,
  );
  assert.deepEqual(readback, normalized);

  console.log('firmware-shaped config export validates and round-trips through App authority');
}

run().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
