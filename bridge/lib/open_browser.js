const { spawn } = require('node:child_process');

function shouldOpenBrowser(argv, packaged = Boolean(process.pkg)) {
  return (
    !argv.includes('--no-open-browser') &&
    (packaged || argv.includes('--open-browser'))
  );
}

function openBrowser(url, injected = {}) {
  const platform = injected.platform || process.platform;
  const spawnImpl = injected.spawn || spawn;
  const [command, args] =
    platform === 'darwin'
      ? ['open', [url]]
      : platform === 'win32'
        ? ['cmd', ['/c', 'start', '', url]]
        : ['xdg-open', [url]];
  const child = spawnImpl(command, args, { detached: true, stdio: 'ignore' });
  child.unref();
}

module.exports = { openBrowser, shouldOpenBrowser };
