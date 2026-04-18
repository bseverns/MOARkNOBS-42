// Reject malformed inbound live-slot commands before they reach serial.
function validateSlotValueCommand(command, maxCmdLen = 128) {
  if (
    !command ||
    command.cmd !== 'SET_SLOT_VALUE' ||
    !Number.isInteger(command.slot) ||
    !Number.isInteger(command.value)
  ) {
    return null;
  }
  if (
    command.slot < 0 ||
    command.slot > 41 ||
    command.value < 0 ||
    command.value > 127
  ) {
    return null;
  }
  const normalized = {
    cmd: command.cmd,
    slot: command.slot,
    value: command.value,
  };
  if (JSON.stringify(normalized).length > maxCmdLen) return null;
  return normalized;
}

// Native firmware live-control verb used by OSC and MIDI input paths.
function formatLiveValueCommand(command) {
  return `SET_SLOT_VALUE,${command.slot},${command.value}`;
}

module.exports = {
  validateSlotValueCommand,
  formatLiveValueCommand,
};
