import test from 'node:test';
import assert from 'node:assert/strict';

import { SpectrumAnalyzer } from '../../js/audio/SpectrumAnalyzer.js';
import { MeterLayout } from '../../js/core/MeterLayout.js';

const EXPECTED_48K_EDGES = [
  3, 4, 6, 8, 12, 18, 27, 40, 59, 88, 130, 192, 284, 421, 623, 922, 1365,
];

const EXPECTED_LABELS = [
  '37', '54', '80', '118', '175', '260', '385', '569',
  '843', '1.2k', '1.8k', '2.7k', '4.1k', '6.0k', '8.9k', '13k',
];

function createAnalyzer() {
  return new SpectrumAnalyzer({ layout: new MeterLayout() });
}

function createNode(fillBins) {
  return {
    frequencyBinCount: 2048,
    getByteFrequencyData(target) {
      fillBins(target);
    },
  };
}

test('48 kHz band edges and display labels preserve the original mapping', () => {
  const analyzer = createAnalyzer();

  analyzer.buildBandEdges(48000);

  assert.deepEqual(analyzer.bandEdges, EXPECTED_48K_EDGES);
  assert.deepEqual(analyzer.labels, EXPECTED_LABELS);
});

test('attach owns a correctly sized bin buffer and rebuilds frequency bands', () => {
  const analyzer = createAnalyzer();
  const node = createNode(() => {});

  analyzer.attach(node, 48000);

  assert.equal(analyzer.node, node);
  assert.ok(analyzer.bins instanceof Uint8Array);
  assert.equal(analyzer.bins.length, 2048);
  assert.deepEqual(analyzer.bandEdges, EXPECTED_48K_EDGES);
});

test('levels attack immediately, apply gain, and clamp at one', () => {
  const analyzer = createAnalyzer();
  const node = createNode(bins => bins.fill(128));
  analyzer.attach(node, 48000);

  analyzer.update(0.5, true);
  assert.equal(analyzer.levels[0], (128 / 255) * 0.5);
  assert.equal(analyzer.peaks[0], analyzer.levels[0]);

  analyzer.update(4, true);
  assert.equal(analyzer.levels[0], 1);
  assert.equal(analyzer.peaks[0], 1);
  assert.equal(analyzer.peakVelocity[0], 0);
});

test('levels release by 22 percent of the remaining distance per frame', () => {
  let byte = 255;
  const analyzer = createAnalyzer();
  analyzer.attach(createNode(bins => bins.fill(byte)), 48000);

  analyzer.update(1, true);
  byte = 0;
  analyzer.update(1, true);
  assert.equal(analyzer.levels[0], 0.78);

  analyzer.update(1, true);
  assert.equal(analyzer.levels[0], 0.6084);
});

test('held peaks fall with accelerating frame-based gravity', () => {
  let byte = 255;
  const analyzer = createAnalyzer();
  analyzer.attach(createNode(bins => bins.fill(byte)), 48000);

  analyzer.update(1, true);
  byte = 0;

  analyzer.update(1, true);
  assert.equal(analyzer.peakVelocity[0], 0.0009);
  assert.equal(analyzer.peaks[0], 0.9991);

  analyzer.update(1, true);
  assert.equal(analyzer.peakVelocity[0], 0.0018);
  assert.equal(analyzer.peaks[0], 0.9973);
});

test('disabling peak hold clears peaks while preserving meter levels', () => {
  const analyzer = createAnalyzer();
  analyzer.attach(createNode(bins => bins.fill(255)), 48000);

  analyzer.update(1, true);
  analyzer.update(1, false);

  assert.equal(analyzer.levels[0], 1);
  assert.equal(analyzer.peaks[0], 0);
});

test('reset methods clear their intended state', () => {
  const analyzer = createAnalyzer();
  analyzer.levels.fill(0.5);
  analyzer.peaks.fill(0.75);
  analyzer.peakVelocity.fill(0.02);

  analyzer.resetPeaks();
  assert.deepEqual(analyzer.levels, new Array(16).fill(0.5));
  assert.deepEqual(analyzer.peaks, new Array(16).fill(0));
  assert.deepEqual(analyzer.peakVelocity, new Array(16).fill(0));

  analyzer.peaks.fill(0.75);
  analyzer.peakVelocity.fill(0.02);
  analyzer.reset();
  assert.deepEqual(analyzer.levels, new Array(16).fill(0));
  assert.deepEqual(analyzer.peaks, new Array(16).fill(0));
  assert.deepEqual(analyzer.peakVelocity, new Array(16).fill(0));
});

test('frame state exposes the analyzer arrays by reference', () => {
  const analyzer = createAnalyzer();
  const frame = analyzer.frameState(true);

  assert.equal(frame.levels, analyzer.levels);
  assert.equal(frame.peaks, analyzer.peaks);
  assert.equal(frame.labels, analyzer.labels);
  assert.equal(frame.peakHoldEnabled, true);
});
