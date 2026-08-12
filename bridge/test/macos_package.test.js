const { strict: assert } = require('node:assert');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

if (process.platform !== 'darwin') {
  console.log('macOS package check skipped off macOS');
  process.exit(0);
}

const outputDir = fs.mkdtempSync(path.join(os.tmpdir(), 'mn42-macos-package-'));
try {
  const result = spawnSync('bash', ['scripts/package_macos.sh'], {
    cwd: path.join(__dirname, '..'),
    encoding: 'utf8',
    env: {
      ...process.env,
      CLI_BINARY: '/usr/bin/true',
      CONSOLE_BINARY: '/usr/bin/true',
      OUTPUT_DIR: outputDir,
      RELEASE_VERSION: '1.2.3',
      REQUIRE_BRIDGE_SIGNING: '0',
    },
  });
  assert.equal(result.status, 0, `${result.stdout}\n${result.stderr}`);

  const app = path.join(outputDir, 'MN42 Bridge.app');
  const architecture = process.arch === 'arm64' ? 'arm64' : 'x64';
  const dmg = path.join(outputDir, `MN42-Bridge-1.2.3-${architecture}.dmg`);
  assert.equal(
    fs.existsSync(path.join(app, 'Contents', 'MacOS', 'MN42 Bridge')),
    true,
  );
  assert.equal(
    fs.existsSync(path.join(app, 'Contents', 'Resources', 'mn42-bridge-cli')),
    true,
  );
  assert.equal(fs.existsSync(dmg), true);

  const verify = spawnSync('hdiutil', ['verify', dmg], { encoding: 'utf8' });
  assert.equal(verify.status, 0, `${verify.stdout}\n${verify.stderr}`);
} finally {
  fs.rmSync(outputDir, { recursive: true, force: true });
}

console.log(
  'macOS package contains the app, both Bridge programs, and a valid DMG',
);
