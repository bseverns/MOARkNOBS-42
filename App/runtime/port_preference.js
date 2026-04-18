export function createPortPreferenceStore({ storage, storageKey } = {}) {
  function getPortInfo(port) {
    if (!port?.getInfo) return null;
    try {
      const info = port.getInfo();
      if (!info) return null;
      return {
        usbVendorId: info.usbVendorId,
        usbProductId: info.usbProductId
      };
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
      return JSON.parse(raw);
    } catch (err) {
      console.debug('read port preference failed', err);
      return null;
    }
  }

  return {
    getPortInfo,
    persist,
    read
  };
}
