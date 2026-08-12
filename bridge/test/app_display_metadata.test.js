const { strict: assert } = require('node:assert');

const {
  normalizeAppDisplayMetadata,
} = require('../lib/config/app_display_metadata');

const metadata = normalizeAppDisplayMetadata({
  profileLabels: ['  Rehearsal  ', 42],
  activeProfile: 0,
  slots: [
    { index: 3, label: ' Lighting wash ', routeDescription: 'TouchDesigner scene blend' },
    { index: -1, label: 'invalid' },
  ],
});

assert.equal(metadata.authority, 'advisory-browser-metadata');
assert.deepEqual(metadata.profileLabels, ['Rehearsal', '']);
assert.deepEqual(metadata.slots, [
  { index: 3, label: 'Lighting wash', routeDescription: 'TouchDesigner scene blend' },
]);
assert.equal(metadata.activeProfile, 0);

console.log('App route labels remain bounded advisory Bridge metadata');
