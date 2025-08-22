const Module = require('module');

function JZZ() {
  return {
    openMidiOut: () => ({
      or() {
        return this;
      },
      send() {},
      on() {},
    }),
    openMidiIn: () => ({
      or() {
        return this;
      },
      connect() {
        return this;
      },
      on() {},
    }),
  };
}

const originalLoad = Module._load;
Module._load = function (request, parent, isMain) {
  if (request === 'jzz') return JZZ;
  return originalLoad(request, parent, isMain);
};

module.exports = JZZ;
