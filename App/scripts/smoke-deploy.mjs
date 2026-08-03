import { access, readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const appRoot = path.resolve(fileURLToPath(new URL('..', import.meta.url)));
const repoRoot = path.resolve(appRoot, '..');
const outputRoot = path.join(repoRoot, 'dist', 'app');
const manifest = JSON.parse(await readFile(path.join(outputRoot, 'deploy-manifest.json'), 'utf8'));
const releaseRoot = path.join(outputRoot, manifest.release_path);

for (const required of ['index.html', 'benzknobz.css', 'config_schema.json', 'views/benzknobz.js', 'runtime.js']) {
  await access(path.join(releaseRoot, required));
}

const rootIndex = await readFile(path.join(outputRoot, 'index.html'), 'utf8');
if (!rootIndex.includes(`<base href="./${manifest.release_path}"`)) {
  throw new Error('Root index does not pin relative assets to the versioned release directory.');
}
if (!rootIndex.includes(`App source: ${manifest.source_id}`)) {
  throw new Error('Root index does not expose the deployment source identity.');
}

const entry = await readFile(path.join(releaseRoot, 'views', 'benzknobz.js'), 'utf8');
const imports = Array.from(entry.matchAll(/from\s+['"]([^'"]+)['"]/g), (match) => match[1]);
for (const specifier of imports.filter((value) => value.startsWith('.'))) {
  await access(path.resolve(releaseRoot, 'views', specifier));
}

console.log(`Deployment smoke check passed for ${manifest.source_id}`);
