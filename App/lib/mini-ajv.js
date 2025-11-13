const isObject = (value) => typeof value === 'object' && value !== null && !Array.isArray(value);

const escapeJsonPointer = (segment) => String(segment).replace(/~/g, '~0').replace(/\//g, '~1');

const joinPath = (base, segment) => {
  if (!segment && segment !== 0) return base;
  const escaped = escapeJsonPointer(segment);
  return `${base}/${escaped}`;
};

const addError = (errors, { instancePath, schemaPath, keyword, message, params = {} }) => {
  errors.push({ instancePath, schemaPath, keyword, message, params });
};

const checkType = (type, data) => {
  switch (type) {
    case 'string':
      return typeof data === 'string';
    case 'number':
      return typeof data === 'number' && Number.isFinite(data);
    case 'integer':
      return typeof data === 'number' && Number.isInteger(data);
    case 'boolean':
      return typeof data === 'boolean';
    case 'object':
      return isObject(data);
    case 'array':
      return Array.isArray(data);
    default:
      return false;
  }
};

const validateSchema = (schema, data, instancePath, schemaPath, errors, options) => {
  if (!schema || typeof schema !== 'object') {
    return;
  }

  const expectedTypes = Array.isArray(schema.type) ? schema.type : schema.type ? [schema.type] : [];
  if (expectedTypes.length) {
    const isValidType = expectedTypes.some((type) => checkType(type, data));
    if (!isValidType) {
      addError(errors, {
        instancePath,
        schemaPath: `${schemaPath}/type`,
        keyword: 'type',
        params: { type: expectedTypes.join(', ') },
        message: `must be of type ${expectedTypes.join(', ')}`
      });
      return;
    }
  }

  if (schema.enum && !schema.enum.includes(data)) {
    addError(errors, {
      instancePath,
      schemaPath: `${schemaPath}/enum`,
      keyword: 'enum',
      params: { allowedValues: schema.enum },
      message: `must be equal to one of: ${schema.enum.join(', ')}`
    });
  }

  if (typeof schema.minimum === 'number' && typeof data === 'number') {
    if (data < schema.minimum) {
      addError(errors, {
        instancePath,
        schemaPath: `${schemaPath}/minimum`,
        keyword: 'minimum',
        params: { limit: schema.minimum },
        message: `must be >= ${schema.minimum} (minimum ${schema.minimum})`
      });
    }
  }

  if (typeof schema.maximum === 'number' && typeof data === 'number') {
    if (data > schema.maximum) {
      addError(errors, {
        instancePath,
        schemaPath: `${schemaPath}/maximum`,
        keyword: 'maximum',
        params: { limit: schema.maximum },
        message: `must be <= ${schema.maximum} (maximum ${schema.maximum})`
      });
    }
  }

  if (typeof schema.maxLength === 'number' && typeof data === 'string') {
    if (data.length > schema.maxLength) {
      addError(errors, {
        instancePath,
        schemaPath: `${schemaPath}/maxLength`,
        keyword: 'maxLength',
        params: { limit: schema.maxLength },
        message: `must NOT be longer than ${schema.maxLength} characters`
      });
    }
  }

  if (typeof schema.pattern === 'string' && typeof data === 'string') {
    const re = new RegExp(schema.pattern);
    if (!re.test(data)) {
      addError(errors, {
        instancePath,
        schemaPath: `${schemaPath}/pattern`,
        keyword: 'pattern',
        params: { pattern: schema.pattern },
        message: `must match pattern ${schema.pattern}`
      });
    }
  }

  if (Array.isArray(schema.required) && isObject(data)) {
    for (const key of schema.required) {
      if (!(key in data)) {
        addError(errors, {
          instancePath,
          schemaPath: `${schemaPath}/required`,
          keyword: 'required',
          params: { missingProperty: key },
          message: `must have required property '${key}'`
        });
      }
    }
  }

  if (schema.additionalProperties === false && isObject(data)) {
    const allowed = new Set(Object.keys(schema.properties || {}));
    for (const key of Object.keys(data)) {
      if (!allowed.has(key)) {
        addError(errors, {
          instancePath,
          schemaPath: `${schemaPath}/additionalProperties`,
          keyword: 'additionalProperties',
          params: { additionalProperty: key },
          message: `must NOT have additional property '${key}'`
        });
      }
    }
  }

  if (schema.properties && isObject(data)) {
    for (const [key, subSchema] of Object.entries(schema.properties)) {
      if (Object.prototype.hasOwnProperty.call(data, key)) {
        validateSchema(
          subSchema,
          data[key],
          joinPath(instancePath, key),
          `${schemaPath}/properties/${escapeJsonPointer(key)}`,
          errors,
          options
        );
      }
    }
  }

  if (schema.type === 'array' && Array.isArray(data)) {
    if (typeof schema.minItems === 'number' && data.length < schema.minItems) {
      addError(errors, {
        instancePath,
        schemaPath: `${schemaPath}/minItems`,
        keyword: 'minItems',
        params: { limit: schema.minItems },
        message: `must NOT have fewer than ${schema.minItems} items`
      });
    }
    if (typeof schema.maxItems === 'number' && data.length > schema.maxItems) {
      addError(errors, {
        instancePath,
        schemaPath: `${schemaPath}/maxItems`,
        keyword: 'maxItems',
        params: { limit: schema.maxItems },
        message: `must NOT have more than ${schema.maxItems} items`
      });
    }
    if (schema.items && typeof schema.items === 'object') {
      data.forEach((item, index) => {
        validateSchema(
          schema.items,
          item,
          joinPath(instancePath, index),
          `${schemaPath}/items`,
          errors,
          options
        );
      });
    }
  }
};

export default class MiniAjv {
  constructor(options = {}) {
    this.options = options;
  }

  compile(schema) {
    const opts = { allErrors: Boolean(this.options.allErrors) };
    const validator = (data) => {
      const errors = [];
      validateSchema(schema, data, '', '#', errors, opts);
      validator.errors = errors.length ? errors : null;
      return errors.length === 0;
    };
    return validator;
  }

  errorsText(errors = []) {
    return errors.map((err) => `${err.instancePath || '/'} ${err.message}`).join(', ');
  }
}
