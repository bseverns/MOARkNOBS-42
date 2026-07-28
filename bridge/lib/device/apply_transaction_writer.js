const APPLY_WRITE_TIMEOUT_MS = 5000;
const NATIVE_SET_ALL_LINE_PACE_MS = 4;

function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function withTimeout(operation, timeoutMs, message) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(message)), timeoutMs);
    timer.unref?.();
    Promise.resolve()
      .then(operation)
      .then(
        (value) => {
          clearTimeout(timer);
          resolve(value);
        },
        (error) => {
          clearTimeout(timer);
          reject(error);
        },
      );
  });
}

// Owns the asynchronous serial-write half of one immutable Apply candidate.
// Device-session state, readback, and structured events remain with the
// caller; this module only guarantees that a cancelled writer cannot resume.
function createApplyTransactionWriter({
  writeApplyLine,
  abortBulkFrame,
  createError,
  onPendingStarted = () => {},
  onPendingCleared = () => {},
  onUncertain = () => {},
  linePaceMs = NATIVE_SET_ALL_LINE_PACE_MS,
} = {}) {
  if (typeof writeApplyLine !== 'function') {
    throw new Error('apply transaction writer requires writeApplyLine');
  }
  if (typeof abortBulkFrame !== 'function') {
    throw new Error('apply transaction writer requires abortBulkFrame');
  }
  if (typeof createError !== 'function') {
    throw new Error('apply transaction writer requires createError');
  }

  let active = null;

  function clear(pending, { cancelled = false } = {}) {
    if (!pending) return false;
    if (cancelled) pending.cancelled = true;
    clearTimeout(pending.timer);
    if (active === pending) {
      active = null;
      onPendingCleared(pending);
    }
    return true;
  }

  function reject(pending, error) {
    if (!clear(pending, { cancelled: true })) return false;
    pending.reject(error);
    return true;
  }

  function complete(pending, result) {
    if (!pending || active !== pending) return false;
    clear(pending);
    pending.resolve(result);
    return true;
  }

  function start({ checksum, seq, stagedConfig, lines, timeoutMs, writeTimeoutMs = APPLY_WRITE_TIMEOUT_MS }) {
    if (active) throw new Error('Apply transaction writer is already active');
    return new Promise((resolve, rejectPromise) => {
      const pending = {
        checksum,
        seq,
        stagedConfig,
        timer: null,
        cancelled: false,
        resolve,
        reject: rejectPromise,
      };
      pending.timer = setTimeout(() => {
        if (active !== pending) return;
        onUncertain(
          'timeout',
          pending,
          createError(
            'apply_timeout',
            'Timed out waiting for device apply ACK',
            { checksum, seq, timeoutMs },
            504,
          ),
        );
      }, timeoutMs);
      active = pending;
      onPendingStarted(pending);

      (async () => {
        try {
          for (let index = 0; index < lines.length; index += 1) {
            await withTimeout(
              () => writeApplyLine(lines[index]),
              writeTimeoutMs,
              `Timed out writing Apply frame after ${writeTimeoutMs}ms`,
            );
            if (pending.cancelled) {
              throw new Error('Apply serial ownership ended before the payload write completed');
            }
            if (index + 1 < lines.length) {
              await delay(linePaceMs);
              if (pending.cancelled) {
                throw new Error('Apply serial ownership ended before the payload write completed');
              }
            }
          }
        } catch (error) {
          // A cancelled write may settle long after readback releases public
          // exclusivity. It must not abort or write into the next transaction.
          if (pending.cancelled) return;
          try {
            await abortBulkFrame();
          } catch {
            // Firmware's frame timeout clears a partial write after disconnect.
          }
          onUncertain(
            'transport_error',
            pending,
            createError(
              'apply_transport_error',
              error?.message || 'Failed to write staged apply payload',
              { checksum, seq },
              503,
            ),
          );
        }
      })();
    });
  }

  return { clear, complete, reject, start };
}

module.exports = {
  APPLY_WRITE_TIMEOUT_MS,
  NATIVE_SET_ALL_LINE_PACE_MS,
  createApplyTransactionWriter,
};
