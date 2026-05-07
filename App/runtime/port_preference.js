export function createPortPreferenceStore({ storage, storageKey } = {}) {
  function normalizeFilter(value) {
    if (!value || typeof value !== 'object') return null;
    const filter = {};
    const usbVendorId = Number(value.usbVendorId);
    const usbProductId = Number(value.usbProductId);
    if (Number.isInteger(usbVendorId) && usbVendorId >= 0 && usbVendorId <= 0xffff) {
      filter.usbVendorId = usbVendorId;
      if (Number.isInteger(usbProductId) && usbProductId >= 0 && usbProductId <= 0xffff) {
        filter.usbProductId = usbProductId;
      }
      return filter;
    }
    if (typeof value.bluetoothServiceClassId === 'string' && value.bluetoothServiceClassId) {
      filter.bluetoothServiceClassId = value.bluetoothServiceClassId;
      return filter;
    }
    return null;
  }

  function getPortInfo(port) {
    if (!port?.getInfo) return null;
    try {
      return normalizeFilter(port.getInfo());
    } catch (_) {
      return null;
    }
  }

  function persist(port) {
    const info = getPortInfo(port);
    if (!info || !storage) return;
    try {
      storage.setItem(storageKey, JSON.stringify(info));
    } catch (err) {
      console.debug('persist port info failed', err);
    }
  }

  function read() {
    if (!storage) return null;
    try {
      const raw = storage.getItem(storageKey);
      if (!raw) return null;
      const filter = normalizeFilter(JSON.parse(raw));
      if (!filter) storage.removeItem?.(storageKey);
      return filter;
    } catch (err) {
      console.debug('read port preference failed', err);
      return null;
    }
  }

  return {
    getPortInfo,
    normalizeFilter,
    persist,
    read
  };
}
