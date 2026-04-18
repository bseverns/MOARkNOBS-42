function clamp(value, min, max) {
  if (!Number.isFinite(value)) return min;
  return Math.min(max, Math.max(min, value));
}

export function coerceIndex(value) {
  const num = Number(value);
  return Number.isInteger(num) && num >= 0 ? num : null;
}

export function extractSlotIndex(payload, fallback) {
  const candidates = [
    payload?.index,
    payload?.slot,
    payload?.slot_index,
    payload?.slotIndex,
    fallback
  ];
  for (const candidate of candidates) {
    const idx = coerceIndex(candidate);
    if (idx !== null) return idx;
  }
  return null;
}

export function normalizeSlotPatchEntry(entry) {
  if (!entry || typeof entry !== 'object') return null;
  const index = coerceIndex(entry.index ?? entry.id ?? entry.slot);
  if (index === null) return null;
  const fields = {};
  if (entry.type !== undefined) fields.type = entry.type;
  else if (entry.type_name !== undefined) fields.type = entry.type_name;
  if (entry.type_code !== undefined) fields.typeCode = entry.type_code;
  const midiChannel = entry.midiChannel ?? entry.channel;
  if (midiChannel !== undefined) {
    const value = Number(midiChannel);
    if (Number.isFinite(value)) fields.midiChannel = value;
  }
  if (entry.data1 !== undefined) {
    const value = Number(entry.data1);
    if (Number.isFinite(value)) fields.data1 = value;
  }
  const efIndexValue =
    entry.efIndex ??
    entry.ef_index ??
    (entry.ef && typeof entry.ef === 'object' ? entry.ef.index : undefined);
  if (efIndexValue !== undefined) {
    const value = Number(efIndexValue);
    if (Number.isFinite(value)) fields.efIndex = value;
  }
  if (entry.ef && typeof entry.ef === 'object') {
    const ef = {};
    const coerceNumber = (value) => {
      const num = Number(value);
      return Number.isFinite(num) ? num : null;
    };
    const pushNumber = (sourceKey, targetKey = sourceKey) => {
      if (entry.ef[sourceKey] === undefined) return;
      const num = coerceNumber(entry.ef[sourceKey]);
      if (num === null) return;
      ef[targetKey] = num;
    };
    if (entry.ef.filter_name !== undefined) {
      ef.filter_name = String(entry.ef.filter_name);
    }
    pushNumber('index');
    pushNumber('filter_index');
    pushNumber('frequency');
    pushNumber('q');
    pushNumber('oversample');
    pushNumber('smoothing');
    pushNumber('baseline');
    pushNumber('gain');
    if (Object.keys(ef).length) {
      fields.ef = ef;
    }
  }
  if (entry.arg && typeof entry.arg === 'object') {
    const arg = {};
    if (entry.arg.enabled !== undefined) {
      const raw = entry.arg.enabled;
      if (typeof raw === 'string') {
        arg.enabled = raw === 'true' || raw === '1';
      } else if (typeof raw === 'number') {
        arg.enabled = raw !== 0;
      } else {
        arg.enabled = Boolean(raw);
      }
    }
    if (entry.arg.method !== undefined) {
      const methodValue = Number(entry.arg.method);
      if (Number.isFinite(methodValue)) arg.method = methodValue;
    }
    if (entry.arg.method_name !== undefined) {
      arg.method_name = String(entry.arg.method_name);
    }
    if (entry.arg.sourceA !== undefined) {
      const sourceA = Number(entry.arg.sourceA);
      if (Number.isFinite(sourceA)) arg.sourceA = sourceA;
    }
    if (entry.arg.sourceB !== undefined) {
      const sourceB = Number(entry.arg.sourceB);
      if (Number.isFinite(sourceB)) arg.sourceB = sourceB;
    }
    if (Object.keys(arg).length) {
      fields.arg = arg;
    }
  }
  if (entry.active !== undefined) fields.active = Boolean(entry.active);
  if (entry.arpNote !== undefined) {
    const value = Number(entry.arpNote);
    if (Number.isFinite(value)) fields.arpNote = value;
  }
  return { index, fields };
}

export function normalizeEfSlotTargets(entry, slotCount) {
  const rawSlots = [];
  if (Array.isArray(entry?.slots)) {
    rawSlots.push(...entry.slots);
  } else {
    const single = entry?.slot ?? entry?.value ?? entry?.target;
    if (single !== undefined && single !== null) rawSlots.push(single);
  }

  const hasSlotLimit = Number.isFinite(Number(slotCount)) && Number(slotCount) > 0;
  const maxSlotIndex = hasSlotLimit ? Number(slotCount) - 1 : Number.MAX_SAFE_INTEGER;
  const seen = new Set();
  const normalized = [];
  rawSlots.forEach((candidate) => {
    const value = Number(candidate);
    if (!Number.isFinite(value)) return;
    const rounded = Math.round(value);
    if (rounded < 0) return;
    const bounded = clamp(rounded, 0, maxSlotIndex);
    if (seen.has(bounded)) return;
    seen.add(bounded);
    normalized.push(bounded);
  });
  normalized.sort((a, b) => a - b);
  return normalized;
}

function sameNumberArray(left, right) {
  if (!Array.isArray(left) || !Array.isArray(right)) return false;
  if (left.length !== right.length) return false;
  for (let idx = 0; idx < left.length; idx += 1) {
    if (Number(left[idx]) !== Number(right[idx])) return false;
  }
  return true;
}

export function applyEfSlotPatch(target, patch, slotCount) {
  if (!Array.isArray(target)) target = [];
  const index = coerceIndex(patch.index);

  if (Array.isArray(patch.slots)) {
    if (index === null) return target;
    const nextSlots = normalizeEfSlotTargets({ slots: patch.slots }, slotCount);
    const prevSlots = normalizeEfSlotTargets(target[index] ?? {}, slotCount);
    if (sameNumberArray(prevSlots, nextSlots)) return target;
    const copy = [...target];
    copy[index] = { ...(copy[index] ?? {}), slots: nextSlots };
    return copy;
  }

  const slotValue = patch.slot ?? patch.value ?? patch.target;
  if (slotValue === undefined) return target;
  const value = Number(slotValue);
  if (!Number.isFinite(value)) return target;
  const rounded = Math.round(value);
  const hasSlotLimit = Number.isFinite(Number(slotCount)) && Number(slotCount) > 0;
  const maxSlotIndex = hasSlotLimit ? Number(slotCount) - 1 : Number.MAX_SAFE_INTEGER;
  if (rounded < 0) return target;
  const boundedSlot = clamp(rounded, 0, maxSlotIndex);

  const copy = target.map((entry) => ({
    ...(entry ?? {}),
    slots: normalizeEfSlotTargets(entry ?? {}, slotCount)
  }));
  let changed = false;

  for (let follower = 0; follower < copy.length; follower += 1) {
    const slots = copy[follower]?.slots ?? [];
    if (!slots.includes(boundedSlot)) continue;
    copy[follower].slots = slots.filter((slot) => slot !== boundedSlot);
    changed = true;
  }

  if (index !== null) {
    const current = copy[index] ?? { slots: [] };
    const nextSet = new Set(current.slots ?? []);
    const beforeSize = nextSet.size;
    nextSet.add(boundedSlot);
    const nextSlots = Array.from(nextSet).sort((a, b) => a - b);
    if (
      !copy[index] ||
      beforeSize !== nextSet.size ||
      !sameNumberArray(current.slots ?? [], nextSlots)
    ) {
      copy[index] = { ...current, slots: nextSlots };
      changed = true;
    }
  }

  return changed ? copy : target;
}

export function createPatchReconciler({
  getLiveConfig,
  getStagedConfig,
  isDirty,
  setLiveConfig,
  setStagedConfig,
  clone,
  normalizeConfig,
  shallowEqual,
  getManifest,
  broadcastConfig
} = {}) {
  return function applyConfigPatch(patch) {
    if (!patch || typeof patch !== 'object' || !getLiveConfig?.()) return;

    const prevLive = clone(getLiveConfig());
    const nextLive = clone(getLiveConfig());
    let mutated = false;
    let filterMeta = null;
    let argMeta = null;

    if (Array.isArray(patch.slots)) {
      const slotsSource = Array.isArray(nextLive.slots) ? [...nextLive.slots] : [];
      let slotsChanged = false;
      patch.slots.forEach((entry) => {
        const normalized = normalizeSlotPatchEntry(entry);
        if (!normalized) return;
        const { index, fields } = normalized;
        const current = { ...(slotsSource[index] ?? {}) };
        let updated = false;
        Object.entries(fields).forEach(([key, value]) => {
          if (value === undefined) return;
          if (value && typeof value === 'object' && !Array.isArray(value)) {
            const existing = current[key] && typeof current[key] === 'object' ? current[key] : {};
            const merged = { ...existing, ...value };
            if (!shallowEqual(existing, merged)) {
              current[key] = merged;
              updated = true;
            }
            return;
          }
          if (current[key] !== value) {
            current[key] = value;
            updated = true;
          }
        });
        if (updated) {
          slotsSource[index] = current;
          slotsChanged = true;
        }
      });
      if (slotsChanged) {
        nextLive.slots = slotsSource;
        mutated = true;
      }
    }

    if (Array.isArray(patch.efSlots)) {
      let efSlots = Array.isArray(nextLive.efSlots) ? nextLive.efSlots : [];
      let efChanged = false;
      patch.efSlots.forEach((entry, idx) => {
        const index = coerceIndex(entry?.index ?? idx);
        const nextArray = applyEfSlotPatch(
          efSlots,
          { ...(entry && typeof entry === 'object' ? entry : {}), index },
          Array.isArray(nextLive.slots) ? nextLive.slots.length : 0
        );
        if (nextArray !== efSlots) {
          efSlots = nextArray;
          efChanged = true;
        }
      });
      if (efChanged) {
        nextLive.efSlots = efSlots;
        mutated = true;
      }
    }

    if (patch.filter && typeof patch.filter === 'object') {
      const prevFilter = prevLive.filter ?? {};
      const nextFilter = { ...(nextLive.filter ?? {}), ...patch.filter };
      if (JSON.stringify(nextFilter) !== JSON.stringify(nextLive.filter ?? {})) {
        nextLive.filter = nextFilter;
        mutated = true;
        filterMeta = { prev: prevFilter, next: nextFilter };
      }
    }

    if (patch.arg && typeof patch.arg === 'object') {
      const prevArg = prevLive.arg ?? {};
      const nextArg = { ...(nextLive.arg ?? {}), ...patch.arg };
      if (JSON.stringify(nextArg) !== JSON.stringify(nextLive.arg ?? {})) {
        nextLive.arg = nextArg;
        mutated = true;
        argMeta = { prev: prevArg, next: nextArg };
      }
    }

    if (!mutated) return;

    const stagedConfig = getStagedConfig?.();
    let nextStaged;
    if (!stagedConfig || !isDirty?.()) {
      nextStaged = clone(nextLive);
    } else {
      nextStaged = clone(stagedConfig);
      if (Array.isArray(patch.slots) && Array.isArray(nextLive.slots)) {
        const prevSlots = Array.isArray(prevLive.slots) ? prevLive.slots : [];
        const stagedSlots = Array.isArray(nextStaged.slots) ? nextStaged.slots : [];
        patch.slots.forEach((entry) => {
          const normalized = normalizeSlotPatchEntry(entry);
          if (!normalized) return;
          const { index, fields } = normalized;
          const prevSlot = prevSlots[index] ?? {};
          const stagedSlot = stagedSlots[index] ?? {};
          const nextSlot = nextLive.slots[index] ?? {};
          const merged = { ...stagedSlot };
          let stagedChanged = false;
          Object.keys(fields).forEach((key) => {
            if (merged[key] === undefined || merged[key] === prevSlot[key]) {
              if (merged[key] !== nextSlot[key]) {
                merged[key] = nextSlot[key];
                stagedChanged = true;
              }
            }
          });
          if (!stagedSlots[index] && Object.keys(nextSlot).length) {
            stagedSlots[index] = clone(nextSlot);
          } else if (stagedChanged) {
            stagedSlots[index] = merged;
          }
        });
        nextStaged.slots = stagedSlots;
      }

      if (Array.isArray(patch.efSlots) && Array.isArray(nextLive.efSlots)) {
        const prevEf = Array.isArray(prevLive.efSlots) ? prevLive.efSlots : [];
        const stagedEf = Array.isArray(nextStaged.efSlots) ? nextStaged.efSlots : [];
        patch.efSlots.forEach((entry, idx) => {
          const index = coerceIndex(entry?.index ?? idx);
          if (index === null) {
            for (let follower = 0; follower < nextLive.efSlots.length; follower += 1) {
              const nextVal = normalizeEfSlotTargets(
                nextLive.efSlots[follower] ?? {},
                nextLive.slots?.length ?? 0
              );
              const prevVal = normalizeEfSlotTargets(
                prevEf[follower] ?? {},
                prevLive.slots?.length ?? 0
              );
              const stagedVal =
                stagedEf[follower] && typeof stagedEf[follower] === 'object'
                  ? normalizeEfSlotTargets(stagedEf[follower], nextStaged.slots?.length ?? 0)
                  : undefined;
              if (stagedVal === undefined || sameNumberArray(stagedVal, prevVal)) {
                stagedEf[follower] = { slots: nextVal };
              }
            }
            return;
          }
          const nextVal = normalizeEfSlotTargets(
            nextLive.efSlots[index] ?? {},
            nextLive.slots?.length ?? 0
          );
          const prevVal = normalizeEfSlotTargets(prevEf[index] ?? {}, prevLive.slots?.length ?? 0);
          const stagedVal =
            stagedEf[index] && typeof stagedEf[index] === 'object'
              ? normalizeEfSlotTargets(stagedEf[index], nextStaged.slots?.length ?? 0)
              : undefined;
          if (stagedVal === undefined || sameNumberArray(stagedVal, prevVal)) {
            stagedEf[index] = { slots: nextVal };
          }
        });
        nextStaged.efSlots = stagedEf;
      }

      if (filterMeta) {
        const { prev, next } = filterMeta;
        nextStaged.filter = nextStaged.filter ?? {};
        Object.keys(next).forEach((key) => {
          if (nextStaged.filter[key] === undefined || nextStaged.filter[key] === prev[key]) {
            nextStaged.filter[key] = next[key];
          }
        });
      }

      if (argMeta) {
        const { prev, next } = argMeta;
        nextStaged.arg = nextStaged.arg ?? {};
        Object.keys(next).forEach((key) => {
          if (nextStaged.arg[key] === undefined || nextStaged.arg[key] === prev[key]) {
            nextStaged.arg[key] = next[key];
          }
        });
      }
    }

    const manifest = getManifest?.() ?? {};
    const normalizedLive = normalizeConfig(nextLive, manifest);
    const normalizedStaged = normalizeConfig(nextStaged, manifest);
    setLiveConfig(clone(normalizedLive));
    setStagedConfig(clone(normalizedStaged));
    broadcastConfig?.();
  };
}
