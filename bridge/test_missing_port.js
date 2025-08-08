const { spawn } = require('child_process');

const child = spawn('node', ['mn42_bridge.js', '--serial', '/dev/notaport']);
let sawError = false;

child.stderr.on('data', data => {
  process.stderr.write(data);
  if (data.toString().includes('serial error')) {
    sawError = true;
    child.kill();
  }
});

setTimeout(() => {
  child.kill();
  if (sawError) {
    console.log('missing port handled');
    process.exit(0);
  } else {
    console.error('missing port not handled');
    process.exit(1);
  }
}, 1000);
