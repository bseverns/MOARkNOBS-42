import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import {
  checkCoordinatorSource,
  COORDINATOR_PATHS
} from '../architecture/coordinator_boundaries.js';

const appRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const violations = [];

for (const relativePath of COORDINATOR_PATHS) {
  const source = await readFile(path.join(appRoot, relativePath), 'utf8');
  violations.push(...checkCoordinatorSource(relativePath, source));
}

if (violations.length) {
  console.error('App coordinator boundary violations:');
  for (const violation of violations) console.error(`  - ${violation}`);
  process.exitCode = 1;
} else {
  console.log(`App coordinator boundaries passed (${COORDINATOR_PATHS.length} coordinators).`);
}

