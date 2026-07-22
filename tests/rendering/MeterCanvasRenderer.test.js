import assert from 'node:assert/strict';
import test from 'node:test';

import { MeterLayout } from '../../js/core/MeterLayout.js';
import { MeterCanvasRenderer } from '../../js/rendering/MeterCanvasRenderer.js';

function createCanvas() {
  const calls = [];
  const context = new Proxy({
    calls,
    measureText(text) {
      calls.push(['measureText', text]);
      return { width: text.length * 10 };
    },
  }, {
    get(target, property) {
      if (property in target) return target[property];
      return (...args) => calls.push([property, ...args]);
    },
    set(target, property, value) {
      target[property] = value;
      calls.push(['set', property, value]);
      return true;
    },
  });
  const canvas = {
    clientWidth: 320,
    clientHeight: 120,
    width: 0,
    height: 0,
    getContext(type) {
      assert.equal(type, '2d');
      return context;
    },
  };
  return { canvas, context, calls };
}

test('resize maintains a DPR-scaled canvas backing store', () => {
  const { canvas } = createCanvas();
  const renderer = new MeterCanvasRenderer({
    canvas,
    layout: new MeterLayout(),
    devicePixelRatio: () => 2,
  });

  renderer.resize();

  assert.equal(canvas.width, 640);
  assert.equal(canvas.height, 240);
});

test('render draws all meter segments before ball and box overlays', () => {
  const layout = new MeterLayout();
  const { canvas, calls } = createCanvas();
  const renderer = new MeterCanvasRenderer({ canvas, layout });
  canvas.width = 1640;
  canvas.height = 560;
  const meterState = {
    levels: new Array(layout.bandCount).fill(0),
    peaks: new Array(layout.bandCount).fill(0),
    labels: new Array(layout.bandCount).fill('100'),
    peakHoldEnabled: true,
  };

  renderer.render(meterState, [
    { kind: 'ball', x: 100, y: 100, angle: 0, size: 20, hue: 220 },
    { kind: 'box', x: 200, y: 100, angle: 0.5, size: 20, hue: 280 },
  ]);

  assert.equal(calls.filter(call => call[0] === 'fillRect').length, 1);
  assert.equal(
    calls.filter(call => call[0] === 'roundRect').length,
    layout.bandCount * layout.segmentCount + 1,
  );
  assert.equal(calls.filter(call => call[0] === 'arc').length, 2);
  assert.ok(calls.some(call =>
    call[0] === 'scale' && call[1] === 1 && call[2] === 1));
});
