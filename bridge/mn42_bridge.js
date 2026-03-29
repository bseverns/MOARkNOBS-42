#!/usr/bin/env node

const {
  createBridgeService,
  parseConfigFromArgv,
  usageText,
} = require('./lib/bridge_service');

// Print the shared bridge usage string for CLI help/error paths.
function printUsage(stream = process.stdout) {
  stream.write(`${usageText()}\n`);
}

// Mirror bridge log events onto stdout/stderr so the CLI behaves like a normal shell tool.
function bindConsoleLogging(service) {
  return service.on('log', (entry) => {
    const line = entry?.message || '';
    if (!line) return;
    if (entry.level === 'error') {
      console.error(line);
      return;
    }
    if (entry.level === 'warn') {
      console.warn(line);
      return;
    }
    console.log(line);
  });
}

// Parse CLI args, start the bridge service, and return the running instance for tests.
async function runCli(argv = process.argv, injected = {}) {
  if (argv.includes('--help') || argv.includes('-h')) {
    printUsage();
    process.exit(0);
  }

  let config;
  try {
    config = parseConfigFromArgv(argv);
  } catch (err) {
    console.error(err.message);
    if (err.showUsage) {
      printUsage(process.stderr);
    }
    process.exit(1);
  }

  const service = createBridgeService(config, injected);
  bindConsoleLogging(service);
  await service.start();
  return service;
}

// Default executable path: boot the CLI bridge and fail loudly on startup errors.
runCli().catch((err) => {
  console.error('bridge failed:', err.message);
  process.exit(1);
});

module.exports = {
  runCli,
};
