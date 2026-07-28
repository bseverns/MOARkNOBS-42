const { strict: assert } = require('node:assert');

const { createApplyTransactionWriter } = require('../lib/device/apply_transaction_writer');

function wait(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function createError(code, message, details, statusCode) {
  const error = new Error(message);
  error.code = code;
  error.details = details;
  error.statusCode = statusCode;
  return error;
}

async function run() {
  {
    const writes = [];
    let pending = null;
    const writer = createApplyTransactionWriter({
      writeApplyLine: async (line) => writes.push(line),
      abortBulkFrame: async () => {},
      createError,
      onPendingStarted: (next) => { pending = next; },
    });
    const apply = writer.start({
      checksum: 'checksum',
      seq: 1,
      stagedConfig: {},
      lines: ['SET_ALL first'],
      timeoutMs: 100,
    });
    await wait(1);
    assert.deepEqual(writes, ['SET_ALL first']);
    assert.equal(writer.complete(pending, { applied: true }), true);
    assert.deepEqual(await apply, { applied: true });
  }

  {
    const writes = [];
    let resolveFirstWrite;
    let uncertain = null;
    let writer;
    writer = createApplyTransactionWriter({
      writeApplyLine: (line) => {
        writes.push(line);
        if (writes.length === 1) {
          return new Promise((resolve) => { resolveFirstWrite = resolve; });
        }
        return undefined;
      },
      abortBulkFrame: async () => {},
      createError,
      onUncertain: (reason, pending, error) => {
        uncertain = { reason, pending };
        writer.reject(pending, error);
      },
    });
    const apply = writer.start({
      checksum: 'checksum',
      seq: 2,
      stagedConfig: {},
      lines: ['SET_ALL first', 'SET_ALL second'],
      timeoutMs: 10,
      writeTimeoutMs: 1000,
    });

    await assert.rejects(() => apply, (error) => error?.code === 'apply_timeout');
    assert.equal(uncertain?.reason, 'timeout');
    resolveFirstWrite();
    await wait(20);
    assert.deepEqual(
      writes,
      ['SET_ALL first'],
      'a delayed callback must not resume the cancelled writer into later frames',
    );
  }

  console.log('apply transaction writer owns completion and permanent cancellation');
}

run().catch((error) => {
  console.error(error);
  process.exit(1);
});
