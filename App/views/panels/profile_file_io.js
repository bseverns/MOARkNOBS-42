export function createProfileFileIO({
  runtime,
  setStatus,
  clampSlot,
  slotCount,
  getActiveProfileSlot,
  slotLabel,
  describeSlot,
  setActiveProfileSlot
} = {}) {
  // Export the currently targeted profile slot as a standalone JSON file backup.
  function handleProfileDownload() {
    const { staged, live } = runtime.getState();
    const payload = staged ?? live;
    if (!payload || typeof payload !== 'object') {
      setStatus('warn', 'Nothing to download', 'Stage a profile before exporting.');
      return;
    }
    const configPayload = {
      slot: clampSlot(getActiveProfileSlot(), slotCount),
      schema_version: runtime.getState().schema?.schema_version,
      timestamp: new Date().toISOString(),
      config: payload
    };
    const blob = new Blob([JSON.stringify(configPayload, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = `moarknobz-profile-${slotLabel(configPayload.slot)}-${new Date()
      .toISOString()
      .replace(/[:.]/g, '-')}.json`;
    anchor.click();
    URL.revokeObjectURL(url);
    setStatus('ok', 'Profile downloaded', `${describeSlot(configPayload.slot)} saved locally.`);
  }

  // Import a profile JSON file into staged state so the user can review diffs first.
  function handleProfileUpload() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json,application/json';
    input.onchange = async () => {
      const file = input.files?.[0];
      if (!file) return;
      try {
        const text = await file.text();
        const documentPayload = JSON.parse(text);
        const configData = documentPayload?.config ?? documentPayload?.profile ?? documentPayload;
        if (!configData || typeof configData !== 'object') {
          throw new Error('File did not contain a config payload');
        }
        const requestedSlot = clampSlot(
          documentPayload?.slot ?? documentPayload?.profile_slot ?? documentPayload?.slotIndex,
          slotCount
        );
        if (Number.isFinite(requestedSlot)) {
          setActiveProfileSlot(requestedSlot);
        }
        // Import is staged-only by design so users can inspect diffs before pushing to hardware.
        runtime.stage(() => configData);
        setStatus(
          'warn',
          'Profile imported',
          `${describeSlot(requestedSlot)} staged. Apply to push it to the deck.`
        );
      } catch (err) {
        setStatus('err', 'Profile import failed', err.message || String(err));
      }
    };
    input.click();
  }

  return {
    handleProfileDownload,
    handleProfileUpload
  };
}
