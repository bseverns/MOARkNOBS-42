import { test, expect } from '@playwright/test';
import { buildModMatrixView } from '../views/panels/mod_matrix_view.js';

test('modulation matrix view filters conflicts and limits the preview without changing its report', () => {
  const report = {
    routes: [
      { id: 'route0', source_type: 'lfo', destination: 'slot0.value', active: true },
      { id: 'route1', source_type: 'midi', destination: 'slot0.value', active: true },
      { id: 'route2', source_type: 'lfo', destination: 'slot1.value', active: false }
    ],
    conflicts: [{ target: 'slot.value', slot: 0, route_ids: ['route0', 'route1'] }]
  };

  const view = buildModMatrixView(report, {
    conflictsOnly: true,
    lfoOnly: true,
    slotOnly: false,
    activeOnly: true
  }, { previewLimit: 1 });

  expect(view.routes).toHaveLength(3);
  expect(view.filteredRoutes.map((route) => route.id)).toEqual(['route0']);
  expect(view.filteredConflicts).toHaveLength(1);
  expect(view.limitedRoutes).toHaveLength(1);
});
