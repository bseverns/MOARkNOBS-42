const { spawn } = require('child_process');

// Spin up the bridge pointed at a deliberately bogus serial port.
// We're simulating the classic "USB cable fell out" scenario to make sure
// the script owns up to the failure instead of hanging like a chump.
const child = spawn('node', ['mn42_bridge.js', '--serial', '/dev/notaport']);
let sawError = false; // flips true once the bridge yells about the missing port

child.stderr.on('data', data => {
  process.stderr.write(data); // surface the whining so the dev sees it
  if (data.toString().includes('serial error')) {
    // The bridge confessed it can't find the port—mission accomplished.
    sawError = true;
    child.kill();
  }
});

setTimeout(() => {
  child.kill();
  // After one second of awkward silence, decide if the bridge flunked or not.
  if (sawError) {
    console.log('missing port handled'); // it screamed on cue, we're good
    process.exit(0);
  } else {
    console.error('missing port not handled'); // no scream means broken error handling
    process.exit(1);
  }
}, 1000);
