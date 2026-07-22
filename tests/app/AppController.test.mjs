import assert from 'node:assert/strict';
import test from 'node:test';

import { AppController } from '../../js/app/AppController.mjs';

class FakeView {
  constructor() {
    this.captureMode = 'mic';
    this.deviceId = '';
    this.gain = 1;
    this.calls = [];
  }

  bindHandlers(handlers) { this.handlers = handlers; }
  setListening(value) { this.calls.push(['listening', value]); }
  setStatus(kind, text) { this.calls.push(['status', kind, text]); }
  setGainLabel(value) { this.calls.push(['gain', value]); }
  setPeakHoldEnabled(value) { this.calls.push(['peak', value]); }
  setPhysicsEnabled(value) { this.calls.push(['physics', value]); }
  setLowGravityEnabled(value) { this.calls.push(['gravity', value]); }
  setPhysicsAvailable(value) { this.calls.push(['physicsAvailable', value]); }
  setDeviceOptions(options, settings) {
    this.calls.push(['devices', options, settings]);
  }
}

function createHarness() {
  const view = new FakeView();
  const audioCapture = {
    running: false,
    startCalls: [],
    stopCalls: 0,
    devices: [],
    async start(options) {
      this.startCalls.push(options);
      this.running = true;
      return { analyserNode: { name: 'analyser' }, sampleRate: 44100 };
    },
    stop() {
      this.stopCalls++;
      this.running = false;
    },
    async listInputDevices() { return this.devices; },
  };
  const analyzer = {
    builtAt: [],
    attachments: [],
    updates: [],
    resetCalls: 0,
    resetPeakCalls: 0,
    state: { levels: [], peaks: [], labels: [], peakHoldEnabled: true },
    buildBandEdges(rate) { this.builtAt.push(rate); },
    attach(node, rate) { this.attachments.push([node, rate]); },
    update(gain, peak) { this.updates.push([gain, peak]); },
    reset() { this.resetCalls++; },
    resetPeaks() { this.resetPeakCalls++; },
    frameState(peakHoldEnabled) {
      return { ...this.state, peakHoldEnabled };
    },
  };
  const physicsWorld = {
    available: true,
    steps: [],
    balls: [],
    boxes: [],
    clearCalls: 0,
    gravity: [],
    objects: [{ kind: 'ball' }],
    step(dt, state) { this.steps.push([dt, state]); },
    spawnBall(x, y) { this.balls.push([x, y]); },
    spawnBox(x, y) { this.boxes.push([x, y]); },
    clear() { this.clearCalls++; },
    setLowGravity(low) { this.gravity.push(low); },
    snapshot() { return this.objects; },
  };
  const renderer = {
    resizeCalls: 0,
    renders: [],
    resize() { this.resizeCalls++; },
    render(state, objects) { this.renders.push([state, objects]); },
  };
  const frames = [];
  const controller = new AppController({
    view,
    audioCapture,
    analyzer,
    physicsWorld,
    renderer,
    requestAnimationFrame: callback => frames.push(callback),
  });

  return { controller, view, audioCapture, analyzer, physicsWorld, renderer, frames };
}

test('start initializes the view, analyzer labels, renderer, and frame loop once', () => {
  const { controller, view, analyzer, renderer, frames } = createHarness();

  controller.start();
  controller.start();

  assert.ok(view.handlers);
  assert.equal(renderer.resizeCalls, 1);
  assert.deepEqual(analyzer.builtAt, [48000]);
  assert.equal(frames.length, 1);
  assert.ok(view.calls.some(call => call[0] === 'status' && call[1] === 'stopped'));
  assert.ok(view.calls.some(call => call[0] === 'physicsAvailable' && call[1] === true));
});

test('capture success attaches the analyzer and stop resets it', async () => {
  const { controller, view, audioCapture, analyzer } = createHarness();

  await controller.startCapture();

  assert.equal(audioCapture.startCalls[0].mode, 'mic');
  assert.deepEqual(analyzer.attachments, [[{ name: 'analyser' }, 44100]]);
  assert.ok(view.calls.some(call => call[0] === 'status' && call[1] === 'live'));

  controller.stopCapture();

  assert.equal(audioCapture.running, false);
  assert.equal(analyzer.resetCalls, 1);
  assert.ok(view.calls.some(call => call[0] === 'status' && call[1] === 'stopped'));
});

test('capture errors stop partial resources and show the original error status', async () => {
  const { controller, view, audioCapture } = createHarness();
  audioCapture.start = async () => { throw new Error('permission denied'); };

  await controller.startCapture();

  assert.equal(audioCapture.stopCalls, 1);
  assert.ok(view.calls.some(call =>
    call[0] === 'status' &&
    call[1] === 'error' &&
    call[2] === '▲ CAPTURE ERROR — permission denied'));
});

test('device options preserve tab and microphone behavior', async () => {
  const { controller, view, audioCapture } = createHarness();
  audioCapture.devices = [
    { label: 'Studio Mic', deviceId: 'studio', kind: 'audioinput' },
    { label: '', deviceId: 'hidden', kind: 'audioinput' },
  ];

  await controller.refreshDeviceList();
  let deviceCall = view.calls.filter(call => call[0] === 'devices').at(-1);
  assert.deepEqual(deviceCall[1], [
    { label: 'Default input', value: '' },
    { label: 'Studio Mic', value: 'studio' },
    { label: 'Microphone', value: 'hidden' },
  ]);

  view.captureMode = 'tab';
  await controller.refreshDeviceList();
  deviceCall = view.calls.filter(call => call[0] === 'devices').at(-1);
  assert.deepEqual(deviceCall[1], [{ label: 'Browser share picker', value: '' }]);
  assert.deepEqual(deviceCall[2], { disabled: true });
});

test('control state and frame orchestration remain centralized', () => {
  const { controller, view, audioCapture, analyzer, physicsWorld, renderer, frames } = createHarness();

  controller.togglePeakHold();
  assert.equal(analyzer.resetPeakCalls, 1);
  assert.equal(controller.peakHoldEnabled, false);

  controller.togglePhysics();
  controller.toggleLowGravity();
  controller.spawnFromCanvas('ball', { x: 10, y: 20 });
  controller.spawnFromCanvas('box', { x: 30, y: 40 });
  assert.deepEqual(physicsWorld.gravity, [true]);
  assert.deepEqual(physicsWorld.balls, [[10, 20]]);
  assert.deepEqual(physicsWorld.boxes, [[30, 40]]);

  audioCapture.running = true;
  view.gain = 1.3;
  controller.frame(1000);

  assert.deepEqual(analyzer.updates, [[1.3, false]]);
  assert.equal(physicsWorld.steps.length, 1);
  assert.deepEqual(renderer.renders[0][1], physicsWorld.objects);
  assert.equal(frames.length, 1);
});
