const { EVENT_CONTRACT_VERSION } = require('../bridge_contract');

function clone(value) {
  return value == null ? value : JSON.parse(JSON.stringify(value));
}

function createStructuredEvent(event, payload = {}) {
  return {
    version: EVENT_CONTRACT_VERSION,
    event,
    at: new Date().toISOString(),
    payload: clone(payload),
  };
}

function isObject(value) {
  return Boolean(value) && typeof value === 'object' && !Array.isArray(value);
}

function validateStructuredEventShape(message) {
  const errors = [];
  if (!isObject(message)) {
    errors.push('event must be an object');
    return errors;
  }
  if (!Number.isInteger(message.version) || message.version < 1) {
    errors.push('version must be a positive integer');
  }
  if (typeof message.event !== 'string' || !message.event.trim()) {
    errors.push('event must be a non-empty string');
  }
  if (typeof message.at !== 'string' || !message.at.trim()) {
    errors.push('at must be an ISO timestamp string');
  }
  if (
    !Object.prototype.hasOwnProperty.call(message, 'payload') ||
    !isObject(message.payload)
  ) {
    errors.push('payload must be an object');
  }
  return errors;
}

module.exports = {
  EVENT_CONTRACT_VERSION,
  createStructuredEvent,
  validateStructuredEventShape,
};
