import test from 'node:test';
import assert from 'node:assert/strict';

import { MeterLayout } from '../../js/core/MeterLayout.mjs';

test('default layout preserves the original logical meter geometry', () => {
  const layout = new MeterLayout();

  assert.equal(layout.logicalWidth, 1640);
  assert.equal(layout.logicalHeight, 560);
  assert.equal(layout.padX, 24);
  assert.equal(layout.padTop, 14);
  assert.equal(layout.padBottom, 46);
  assert.equal(layout.bandCount, 16);
  assert.equal(layout.segmentCount, 28);
  assert.equal(layout.segmentGap, 4);
  assert.equal(layout.usableWidth, 1592);
  assert.equal(layout.usableHeight, 500);
  assert.equal(layout.segmentHeight, 14);
  assert.equal(layout.columnWidth, 99.5);
  assert.equal(layout.barWidth, 61.69);
  assert.equal(layout.groundY, 514);
  assert.equal(Object.isFrozen(layout), true);
});

test('bar positions span the default 16-column layout exactly', () => {
  const layout = new MeterLayout();

  assert.equal(layout.barCenterX(0), 73.75);
  assert.equal(layout.barCenterX(7), 770.25);
  assert.equal(layout.barCenterX(15), 1566.25);
});

test('level conversion preserves rounding and top-position clamping', () => {
  const layout = new MeterLayout();

  assert.equal(layout.litSegments(0), 0);
  assert.equal(layout.litSegments(1 / 56), 1);
  assert.equal(layout.litSegments(0.5), 14);
  assert.equal(layout.litSegments(1), 28);

  assert.equal(layout.barTopForLevel(-1), 514);
  assert.equal(layout.barTopForLevel(0), 514);
  assert.equal(layout.barTopForLevel(1 / 56), 500);
  assert.equal(layout.barTopForLevel(0.5), 266);
  assert.equal(layout.barTopForLevel(1), 14);
  assert.equal(layout.barTopForLevel(1.1), 14);
});

test('derived geometry follows custom dimensions and counts', () => {
  const layout = new MeterLayout({
    logicalWidth: 200,
    logicalHeight: 120,
    padX: 10,
    padTop: 5,
    padBottom: 15,
    bandCount: 2,
    segmentCount: 5,
    segmentGap: 2,
    barWidthRatio: 0.5,
  });

  assert.equal(layout.usableWidth, 180);
  assert.equal(layout.usableHeight, 100);
  assert.equal(layout.segmentHeight, 18.4);
  assert.equal(layout.columnWidth, 90);
  assert.equal(layout.barWidth, 45);
  assert.equal(layout.groundY, 105);
  assert.equal(layout.barCenterX(1), 145);
  assert.ok(Math.abs(layout.barTopForLevel(0.4) - 66.2) < Number.EPSILON * 100);
});
