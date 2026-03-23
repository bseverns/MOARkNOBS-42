#!/usr/bin/env node

const {
  createBridgeService,
  parseConfigFromArgv,
  usageText,
} = require('./lib/bridge_service');

function printUsage(stream = process.stdout) {
  stream.write(`${usageText()}\n`);
}

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

runCli().catch((err) => {
  console.error('bridge failed:', err.message);
  process.exit(1);
});

module.exports = {
  runCli,
};
