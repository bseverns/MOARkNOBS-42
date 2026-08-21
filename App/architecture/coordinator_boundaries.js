const COORDINATOR_RULES = {
  'runtime.js': {
    importAllowed(specifier) {
      return (
        specifier.startsWith('./runtime/') ||
        [
          './lib/mini-ajv.js',
          './lib/add-formats.js',
          './lib/constants.js',
          './manifest_contract.js'
        ].includes(specifier)
      );
    },
    importMessage:
      'runtime.js may compose runtime services plus the validator, shared constants, and manifest factory only',
    policyRules: [
      [/[.]capabilities(?:[?][.][\w$]+|[.][\w$]+)/, 'device capability decisions belong in a runtime policy module'],
      [/\bcapabilities\s*:\s*\{/, 'capability tables belong outside the coordinator'],
      [/\bbridgeFailureClass\b/, 'Bridge failure classification belongs in the Bridge config lane'],
      [/\brollbackPolicy\b/, 'RPC rollback semantics belong in the RPC sender'],
      [/\b(?:schemaMigrationRequired|supportsChunkedConfig)\b/, 'schema/device policy belongs in contract_policy.js'],
      [
        /\b(?:configSession|bridgeSessionRuntime)[.]\b(?:apply|rollback|resynchronize|isHealthy|flushStageSync|suspendStageSync|resumeStageSync)\s*\(/,
        'Apply transaction sequencing belongs in apply_coordinator.js'
      ],
      [/\bresult[?]?[.]applied\b/, 'Apply receipt interpretation belongs in the Bridge config lane'],
      [/\bcontractQuality\s*(?:===|!==)/, 'contract-quality decisions belong in contract_policy.js'],
      [/\bincludes\s*\(\s*contractQuality\s*\)/, 'contract-quality decisions belong in contract_policy.js']
    ]
  },
  'views/benzknobz.js': {
    importAllowed(specifier) {
      if (specifier.startsWith('./')) return true;
      return [
        '../runtime.js',
        '../lib/constants.js',
        '../lib/tuning_catalog.js',
        '../manifest_contract.js'
      ].includes(specifier);
    },
    importMessage:
      'benzknobz.js may compose view modules and the public runtime/shared presentation APIs only',
    policyRules: [
      [/[.]capabilities(?:[?][.][\w$]+|[.][\w$]+)/, 'device capability policy belongs in a controller or runtime module'],
      [/\bcapabilities\s*:\s*\{/, 'capability tables belong outside the view coordinator'],
      [/\b(?:bridgeFailureClass|rollbackPolicy)\b/, 'transaction semantics belong outside the view coordinator'],
      [
        /\b(?:minimum|maximum|required|additionalProperties)\s*:/,
        'schema constraints belong in the canonical schema, not the view coordinator'
      ]
    ]
  }
};

function importSpecifiers(source) {
  const imports = [];
  const pattern = /^\s*import\s+(?:[^'\"]*?\s+from\s+)?['\"]([^'\"]+)['\"]\s*;?/gm;
  for (const match of source.matchAll(pattern)) imports.push(match[1]);
  return imports;
}

function lineForOffset(source, offset) {
  return source.slice(0, offset).split('\n').length;
}

export function checkCoordinatorSource(relativePath, source) {
  const rules = COORDINATOR_RULES[relativePath];
  if (!rules) throw new Error(`No coordinator boundary is defined for ${relativePath}`);
  const violations = [];

  for (const specifier of importSpecifiers(source)) {
    if (!rules.importAllowed(specifier)) {
      violations.push(`${relativePath}: import '${specifier}' is outside the boundary: ${rules.importMessage}`);
    }
  }

  for (const [pattern, message] of rules.policyRules) {
    const match = pattern.exec(source);
    pattern.lastIndex = 0;
    if (match) {
      violations.push(`${relativePath}:${lineForOffset(source, match.index)}: ${message}`);
    }
  }
  return violations;
}

export const COORDINATOR_PATHS = Object.freeze(Object.keys(COORDINATOR_RULES));

