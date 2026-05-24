const { strict: assert } = require('node:assert');

const {
  createDefaultManifest,
  createSimulatedMn42Device,
} = require('../lib/device/simulator');

function wait(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function run() {
  const lines = [];
  const simulator = createSimulatedMn42Device({
    manifest: createDefaultManifest({ schema_version: 7 }),
    ackDelayMs: 10,
  });
  simulator.on('line', (line) => lines.push(JSON.parse(line)));

  simulator.receiveLine('HELLO');
  simulator.receiveLine('GET_MANIFEST');
  simulator.receiveLine('GET_SCHEMA');
  simulator.receiveLine('GET_CONFIG');
  simulator.receiveLine(
    'SET_ALL {"seq":1,"checksum":"ok","config":{"slots":[]}}',
  );
  await wait(25);

  assert.equal(lines[0].hello, 'mn42', 'simulator should answer HELLO');
  assert.equal(
    lines.find((entry) => entry.device_name === 'MOARkNOBS-42')?.schema_version,
    7,
    'simulator should support schema-mismatch manifest scenarios',
  );
  assert.equal(
    Boolean(lines.find((entry) => entry.type === 'ack')),
    true,
    'simulator should emit delayed ACKs for SET_ALL payloads',
  );

  lines.length = 0;
  simulator.setAckMode('bad-ack');
  simulator.receiveLine(
    'SET_ALL {"seq":2,"checksum":"expected","config":{"slots":[]}}',
  );
  await wait(15);
  assert.equal(
    lines.find((entry) => entry.type === 'ack')?.checksum,
    'bad-checksum',
    'simulator should emit bad ACKs on demand',
  );

  lines.length = 0;
  simulator.disconnect();
  simulator.receiveLine('GET_MANIFEST');
  await wait(10);
  assert.equal(
    lines.length,
    0,
    'disconnect should stop simulated device replies',
  );

  simulator.connect();
  simulator.receiveLine('GET_MANIFEST');
  await wait(10);
  assert.equal(
    lines.find((entry) => entry.device_name === 'MOARkNOBS-42')?.device_name,
    'MOARkNOBS-42',
    'reconnect should restore simulated device replies',
  );

  console.log(
    'simulated MN42 device covers handshake, ACK modes, and reconnects',
  );
}

run().catch((error) => {
  console.error(error);
  process.exit(1);
});
