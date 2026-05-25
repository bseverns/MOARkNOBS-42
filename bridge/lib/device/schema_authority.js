const fs = require('node:fs/promises');
const fsSync = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

const SNAPSHOT_APP_ROOT = path.resolve(__dirname, '..', '..', '..', 'App');
const PACKAGED_APP_ROOT = path.join(
  os.tmpdir(),
  'mn42-bridge-runtime',
  'App-authority',
);
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
const APP_RUNTIME_FILES = [
  'config_schema.json',
  path.join('lib', 'mini-ajv.js'),
  path.join('lib', 'add-formats.js'),
  path.join('lib', 'constants.js'),
  path.join('runtime', 'config_normalize.js'),
  path.join('runtime', 'patch_reconcile.js'),
  path.join('runtime', 'config_session.js'),
];

function clone(value) {
  return value == null ? value : JSON.parse(JSON.stringify(value));
}

let authorityPromise = null;
let packagedAppRootPromise = null;

async function copyAppRuntimeFile(relativePath) {
  const sourcePath = path.join(SNAPSHOT_APP_ROOT, relativePath);
  const targetPath = path.join(PACKAGED_APP_ROOT, relativePath);
  await fs.mkdir(path.dirname(targetPath), { recursive: true });
  const body = await fs.readFile(sourcePath, 'utf8');
  const output =
    path.extname(relativePath).toLowerCase() === '.js'
      ? transpileEsmToCjs(body)
      : body;
  await fs.writeFile(targetPath, output);
}

function transpileEsmToCjs(source) {
  const namedExports = new Set();
  let defaultExport = null;
  let output = source;

  output = output.replace(
    /^import\s+\{([^}]+)\}\s+from\s+['"](.+)['"];?$/gm,
    (_, names, specifier) => `const {${names}} = require('${specifier}');`,
  );
  output = output.replace(
    /^import\s+([A-Za-z0-9_$]+)\s+from\s+['"](.+)['"];?$/gm,
    (_, localName, specifier) =>
      `const ${localName}Module = require('${specifier}');\nconst ${localName} = ${localName}Module && ${localName}Module.default ? ${localName}Module.default : ${localName}Module;`,
  );
  output = output.replace(
    /^export default class\s+([A-Za-z0-9_$]+)/m,
    (_, name) => {
      defaultExport = name;
      return `class ${name}`;
    },
  );
  output = output.replace(
    /^export default function\s+([A-Za-z0-9_$]+)\s*\(/m,
    (_, name) => {
      defaultExport = name;
      return `function ${name}(`;
    },
  );
  output = output.replace(
    /^export function\s+([A-Za-z0-9_$]+)\s*\(/gm,
    (_, name) => {
      namedExports.add(name);
      return `function ${name}(`;
    },
  );
  output = output.replace(
    /^export const\s+([A-Za-z0-9_$]+)\s*=/gm,
    (_, name) => {
      namedExports.add(name);
      return `const ${name} =`;
    },
  );

  const footer = [];
  if (namedExports.size) {
    footer.push(`module.exports = { ${[...namedExports].join(', ')} };`);
  }
  if (defaultExport) {
    if (!namedExports.size) {
      footer.push(`module.exports = ${defaultExport};`);
    }
    footer.push(`module.exports.default = ${defaultExport};`);
  }
  return `${output}\n${footer.join('\n')}\n`;
}

function loadPackagedModule(appRoot, ...segments) {
  const target = path.join(appRoot, ...segments);
  const mod = require(target);
  return mod && mod.default ? { ...mod, default: mod.default } : mod;
}

async function loadSourceModule(appRoot, ...segments) {
  const target = path.join(appRoot, ...segments);
  return import(pathToFileURL(target).href);
}

async function loadAppModule(appRoot, ...segments) {
  if (process.pkg) {
    return loadPackagedModule(appRoot, ...segments);
  }
  return loadSourceModule(appRoot, ...segments);
}

async function ensurePackagedAppRoot() {
  if (!process.pkg) return SNAPSHOT_APP_ROOT;
  if (!packagedAppRootPromise) {
    packagedAppRootPromise = (async () => {
      if (
        !fsSync.existsSync(path.join(PACKAGED_APP_ROOT, 'config_schema.json'))
      ) {
        await Promise.all(
          APP_RUNTIME_FILES.map((relativePath) =>
            copyAppRuntimeFile(relativePath),
          ),
        );
      }
      return PACKAGED_APP_ROOT;
    })();
  }
  return packagedAppRootPromise;
}

async function loadSchemaAuthority() {
  if (authorityPromise) return authorityPromise;
  authorityPromise = (async () => {
    const appRoot = await ensurePackagedAppRoot();
    const schemaPath = path.join(appRoot, 'config_schema.json');
    const [
      schemaText,
      miniAjvModule,
      addFormatsModule,
      normalizeModule,
      sessionModule,
    ] = await Promise.all([
      fs.readFile(schemaPath, 'utf8'),
      loadAppModule(appRoot, 'lib', 'mini-ajv.js'),
      loadAppModule(appRoot, 'lib', 'add-formats.js'),
      loadAppModule(appRoot, 'runtime', 'config_normalize.js'),
      loadAppModule(appRoot, 'runtime', 'config_session.js'),
    ]);

    const schema = JSON.parse(schemaText);
    const MiniAjv = miniAjvModule.default ?? miniAjvModule;
    const addFormats = addFormatsModule.default ?? addFormatsModule;
    const normalizeConfig = normalizeModule.normalizeConfig;
    const compactConfigForDevice = sessionModule.compactConfigForDevice;
    const slotTypeNames =
      schema?.properties?.slots?.items?.properties?.type?.enum ??
      DEFAULT_SLOT_TYPE_NAMES;

    const ajv = new MiniAjv({ allErrors: true });
    addFormats(ajv);
    const validator = ajv.compile(schema);

    return {
      appRoot,
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
  APP_ROOT: SNAPSHOT_APP_ROOT,
  DEFAULT_SLOT_TYPE_NAMES,
  loadSchemaAuthority,
};
