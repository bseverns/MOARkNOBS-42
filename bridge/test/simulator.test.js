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

  lines.length = 0;
  simulator.receiveLine('GET_MANIFEST');
  const capabilities = lines.at(-1).capabilities;
  assert.deepEqual(
    capabilities,
    {
      profile_save: true,
      profile_load: true,
      profile_reset: true,
      macro_snapshot: false,
      scenes: false,
      arp_live: true,
    },
    'simulator capabilities should describe only implemented feature protocols',
  );

  lines.length = 0;
  simulator.receiveLine('SET_PROFILE,1,{"arp":{"pattern_length":1,"shape":3}}');
  simulator.receiveLine('GET_PROFILE,1');
  assert.equal(lines[0].command, 'SET_PROFILE');
  assert.equal(lines[0].status, 'ok');
  assert.equal(lines[1].arp.pattern_length, 2);
  assert.equal(lines[1].arp.shape, 3);
  simulator.receiveLine('SET_PROFILE_CHUNK,1,0,2,{"arp":{"pattern_');
  simulator.receiveLine('SET_PROFILE_CHUNK,1,1,2,length":99}}');
  simulator.receiveLine('GET_PROFILE,1');
  assert.equal(lines[2].command, 'SET_PROFILE');
  assert.equal(lines[3].arp.pattern_length, 16);
  assert.equal(
    lines[3].arp.shape,
    3,
    'sparse profile patches should preserve existing profile fields',
  );

  lines.length = 0;
  simulator.receiveLine('SET_ARP,6,4,30,75,2,10');
  simulator.receiveLine('SET_ARP,12,1,10,50,1');
  simulator.receiveLine('GET_ARP');
  assert.equal(lines[0].pattern_length, 10);
  assert.equal(lines[1].pattern_length, 10);
  assert.equal(lines[2].pattern_length, 10);

  lines.length = 0;
  simulator.receiveLine('SAVE_PROFILE,2');
  simulator.receiveLine('SET_PROFILE,0,{"arp":{"pattern_length":7}}');
  simulator.receiveLine('LOAD_PROFILE,2');
  simulator.receiveLine('GET_ARP');
  simulator.receiveLine('RESET_PROFILE,2');
  assert.deepEqual(lines[0], {
    profile_saved: true,
    profile: 2,
    active_profile: 0,
  });
  assert.deepEqual(lines[2], {
    profile_loaded: true,
    profile: 2,
    active_profile: 2,
  });
  assert.equal(lines[3].pattern_length, 10);
  assert.deepEqual(lines[4], {
    profile_reset: true,
    profile: 2,
    active_profile: 2,
  });

  console.log(
    'simulated MN42 device covers handshake, config, profile, arp, ACK, and reconnect behavior',
  );
}

run().catch((error) => {
  console.error(error);
  process.exit(1);
});
