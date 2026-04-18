function detachListeners(target) {
  if (target && typeof target.removeAllListeners === 'function') {
    target.removeAllListeners();
  }
}

async function closeSerialDevice(serial, { onCloseError } = {}) {
  if (!serial || typeof serial.close !== 'function') return;
  try {
    await new Promise((resolve) => {
      let settled = false;
      const finish = () => {
        if (settled) return;
        settled = true;
        resolve();
      };

      try {
        const maybe = serial.close((err) => {
          if (err && typeof onCloseError === 'function') {
            onCloseError(err);
          }
          finish();
        });
        if (maybe && typeof maybe.then === 'function') {
          maybe.then(finish).catch(() => finish());
        } else if (serial.close.length === 0) {
          finish();
        }
      } catch (_) {
        finish();
      }
    });
  } catch (_) {
    // no-op
  }
}

function closeQuietly(resource) {
  if (!resource || typeof resource.close !== 'function') return;
  try {
    resource.close();
  } catch (_) {
    // no-op
  }
}

module.exports = {
  detachListeners,
  closeSerialDevice,
  closeQuietly,
};
