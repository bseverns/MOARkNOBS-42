import { test, expect } from '@playwright/test';
import fs from 'node:fs';
import path from 'node:path';
import {
  checkCoordinatorSource,
  COORDINATOR_PATHS
} from '../architecture/coordinator_boundaries.js';

test('large App coordinators remain inside their dependency and policy boundaries', () => {
  const violations = COORDINATOR_PATHS.flatMap((relativePath) =>
    checkCoordinatorSource(
      relativePath,
      fs.readFileSync(path.resolve(process.cwd(), relativePath), 'utf8')
    )
  );
  expect(violations).toEqual([]);
});

test('coordinator guard rejects authority bypasses and embedded policy', () => {
  expect(
    checkCoordinatorSource(
      'views/benzknobz.js',
      "import { normalizeConfig } from '../runtime/config_normalize.js';\n"
    )
  ).toEqual([
    expect.stringContaining("import '../runtime/config_normalize.js' is outside the boundary")
  ]);

  expect(
    checkCoordinatorSource(
      'runtime.js',
      "const allowed = manifest.capabilities?.verified_apply;\n"
    )
  ).toEqual([expect.stringContaining('device capability decisions')]);

  expect(
    checkCoordinatorSource(
      'runtime.js',
      "configSession.apply();\n"
    )
  ).toEqual([expect.stringContaining('Apply transaction sequencing')]);
});

