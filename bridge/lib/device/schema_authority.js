const fs = require('node:fs/promises');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

const APP_ROOT = path.resolve(__dirname, '..', '..', '..', 'App');
const DEFAULT_SLOT_TYPE_NAMES = [
  'OFF',
  'CC',
  'Note',
  'PitchBend',
  'ProgramChange',
  'Aftertouch',
  'ModWheel',
  'NRPN',
  'RPN',
  'SysEx',
];

function clone(value) {
  return value == null ? value : JSON.parse(JSON.stringify(value));
}

let authorityPromise = null;

async function importAppModule(...segments) {
  const target = path.join(APP_ROOT, ...segments);
  return import(pathToFileURL(target).href);
}

async function loadSchemaAuthority() {
  if (authorityPromise) return authorityPromise;
  authorityPromise = (async () => {
    const schemaPath = path.join(APP_ROOT, 'config_schema.json');
    const [
      schemaText,
      miniAjvModule,
      addFormatsModule,
      normalizeModule,
      sessionModule,
    ] = await Promise.all([
      fs.readFile(schemaPath, 'utf8'),
      importAppModule('lib', 'mini-ajv.js'),
      importAppModule('lib', 'add-formats.js'),
      importAppModule('runtime', 'config_normalize.js'),
      importAppModule('runtime', 'config_session.js'),
    ]);

    const schema = JSON.parse(schemaText);
    const MiniAjv = miniAjvModule.default;
    const addFormats = addFormatsModule.default;
    const normalizeConfig = normalizeModule.normalizeConfig;
    const compactConfigForDevice = sessionModule.compactConfigForDevice;
    const slotTypeNames =
      schema?.properties?.slots?.items?.properties?.type?.enum ??
      DEFAULT_SLOT_TYPE_NAMES;

    const ajv = new MiniAjv({ allErrors: true });
    addFormats(ajv);
    const validator = ajv.compile(schema);

    return {
      appRoot: APP_ROOT,
      schema,
      schemaVersion:
        schema?.schema_version ??
        schema?.properties?.schema_version?.default ??
        null,
      slotTypeNames: clone(slotTypeNames),
      normalizeConfig,
      compactConfigForDevice,
      validateConfig(config) {
        const valid = validator(config);
        return {
          valid,
          errors: valid ? [] : clone(validator.errors || []),
        };
      },
    };
  })();
  return authorityPromise;
}

module.exports = {
  APP_ROOT,
  DEFAULT_SLOT_TYPE_NAMES,
  loadSchemaAuthority,
};
