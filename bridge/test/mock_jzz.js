function JZZ() {
  return {
    openMidiOut: () => ({ or() { return this; }, send() {}, on() {} }),
    openMidiIn: () => ({ or() { return this; }, connect() { return this; }, on() {} }),
  };
}
module.exports = JZZ;
