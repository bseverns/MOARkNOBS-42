#!/usr/bin/env node

const {
  createBridgeService,
  parseConfigFromArgv,
} = require('./lib/bridge_service');
const { createBrowserBridgeServer } = require('./lib/http_bridge_server');

// Browser-console-specific help text. The core transport flags still come from bridge_service.
function usageText() {
  return (
    'mn42_bridge_server.js - browser-driven MOARkNOBS-42 bridge\n' +
    'Usage: node mn42_bridge_server.js [--config FILE] [--http-host ADDR] [--http-port PORT] [--serial PORT] [--osc PORT] [--osc-listen PORT] [--host ADDR] [--bind ADDR] [--midi LABEL] [--allow-feedback-loops] [--feedback-window-ms N] [--rt-p95-target-ms N] [--rt-jitter-p95-target-ms N] [--alert-suppression-ms N]'
  );
}

// Print server usage to stdout/stderr without duplicating the text assembly logic.
function printUsage(stream = process.stdout) {
  stream.write(`${usageText()}\n`);
}

// Stream bridge logs into the server process console for local operators.
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

// Start the bridge service plus the HTTP/WebSocket wrapper used by the browser console.
async function runServer(argv = process.argv, injected = {}) {
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
  const server = createBrowserBridgeServer({
    service,
    host: config.httpHost,
    port: config.httpPort,
  });

  const address = await server.start();
  console.log(`bridge console: ${address.url}`);

  let shuttingDown = false;
  // Shared SIGINT/SIGTERM shutdown path so the HTTP wrapper and bridge stop together.
  const shutdown = async () => {
    if (shuttingDown) return;
    shuttingDown = true;
    try {
      await server.stop();
    } finally {
      await service.stop().catch(() => {});
    }
    process.exit(0);
  };

  process.once('SIGINT', shutdown);
  process.once('SIGTERM', shutdown);

  return { service, server, address };
}

// Default executable path for the browser-driven bridge server.
runServer().catch((err) => {
  console.error('bridge server failed:', err.message);
  process.exit(1);
});

module.exports = {
  runServer,
};
