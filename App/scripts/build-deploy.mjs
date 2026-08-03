import { cp, mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const appRoot = path.resolve(fileURLToPath(new URL('..', import.meta.url)));
const repoRoot = path.resolve(appRoot, '..');
const outputRoot = path.join(repoRoot, 'dist', 'app');

function resolveSource() {
  const supplied = String(process.env.APP_SOURCE_SHA ?? '').trim();
  const sha = supplied || execFileSync('git', ['rev-parse', '--short=12', 'HEAD'], {
    cwd: repoRoot,
    encoding: 'utf8'
  }).trim();
  const dirty = process.env.APP_SOURCE_DIRTY === '1' || (!supplied && Boolean(execFileSync(
    'git',
    ['status', '--porcelain', '--untracked-files=all', '--', 'App'],
    { cwd: repoRoot, encoding: 'utf8' }
  ).trim()));
  return { sha, dirty };
}

const source = resolveSource();
const sourceSha = source.sha.replace(/[^a-zA-Z0-9._-]/g, '-');
const sourceId = `${sourceSha}${source.dirty ? '-dirty' : ''}`;
const packageMetadata = JSON.parse(await readFile(path.join(appRoot, 'package.json'), 'utf8'));
const releaseRoot = path.join(outputRoot, 'releases', sourceId);
const excluded = new Set([
  'node_modules',
  'test-results',
  'playwright-report',
  'tests',
  'scripts',
  'package.json',
  'package-lock.json',
  'playwright.config.js',
  'README.md',
  'DEPLOY.md'
]);

await rm(outputRoot, { recursive: true, force: true });
await mkdir(releaseRoot, { recursive: true });
await cp(appRoot, releaseRoot, {
  recursive: true,
  filter: (source) => source === appRoot || !excluded.has(path.basename(source))
});

const releaseIndexPath = path.join(releaseRoot, 'index.html');
const releaseIndex = (await readFile(releaseIndexPath, 'utf8')).replace(
  'App source: development',
  `App source: ${sourceId}`
);
await writeFile(releaseIndexPath, releaseIndex);

const rootIndex = releaseIndex.replace(
  '<head>',
  `<head>\n  <base href="./releases/${sourceId}/" />`
);
await writeFile(path.join(outputRoot, 'index.html'), rootIndex);
await writeFile(
  path.join(outputRoot, 'deploy-manifest.json'),
  `${JSON.stringify({
    app_version: packageMetadata.version,
    source_sha: sourceSha,
    source_id: sourceId,
    working_tree_dirty: source.dirty,
    publishable: !source.dirty,
    release_path: `releases/${sourceId}/`
  }, null, 2)}\n`
);

console.log(`Built cache-safe App artifact for ${sourceId} at ${outputRoot}`);
