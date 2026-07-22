import assert from 'node:assert/strict';
import test from 'node:test';

import { MeterLayout } from '../../js/core/MeterLayout.mjs';
import {
  MatterPhysicsWorld,
  PHYSICS_TIMING,
} from '../../js/physics/MatterPhysicsWorld.mjs';

test('missing Matter.js produces a safe, substitutable no-op world', () => {
  const layout = new MeterLayout();
  const world = new MatterPhysicsWorld({ Matter: undefined, layout });

  assert.equal(world.available, false);
  assert.doesNotThrow(() => {
    world.step(1 / 60, { levels: [], peaks: [], peakHoldEnabled: true });
    world.spawnBall(100, 100);
    world.spawnBox(100, 100);
    world.setLowGravity(true);
    world.clear();
  });
  assert.deepEqual(world.snapshot(), []);
});

test('surface geometry is derived from the shared meter layout', () => {
  const layout = new MeterLayout();
  const world = new MatterPhysicsWorld({ Matter: undefined, layout });
  world.barTops[0] = 100;
  world.barTops[1] = 200;

  assert.equal(world.surfaceYAt(layout.barCenterX(0)), 100);
  assert.equal(
    world.surfaceYAt(layout.barCenterX(0) + layout.columnWidth / 2),
    200,
  );
  assert.equal(world.surfaceYAt(0), null);
});

test('safe spawn height respects overlapping bars and explicit higher positions', () => {
  const layout = new MeterLayout();
  const world = new MatterPhysicsWorld({ Matter: undefined, layout });
  world.barTops[0] = 100;

  assert.equal(world.safeSpawnY(layout.barCenterX(0), 20, 300), 74);
  assert.equal(world.safeSpawnY(layout.barCenterX(0), 20, -1), -1);
});

test('fixed-step timing constants preserve the original simulation contract', () => {
  assert.equal(PHYSICS_TIMING.fixedDt, 1 / 60);
  assert.equal(PHYSICS_TIMING.maxFrameDt, 0.05);
});
