/**
 * Applies the meter's frequency grouping and frame-based level ballistics to
 * byte-frequency data supplied by a Web Audio AnalyserNode.
 */
export class SpectrumAnalyzer {
  static #DEFAULTS = Object.freeze({
    fftSize: 4096,
    minFrequency: 30,
    maxFrequency: 16000,
  });

  static #BALLISTICS = Object.freeze({
    release: 0.22,
    peakAcceleration: 0.0009,
  });

  layout;
  fftSize;
  minFrequency;
  maxFrequency;
  node = null;
  bins = null;
  bandEdges;
  levels;
  peaks;
  peakVelocity;
  labels;

  static get DEFAULT_FFT_SIZE() {
    return SpectrumAnalyzer.#DEFAULTS.fftSize;
  }

  static get DEFAULT_MIN_FREQUENCY() {
    return SpectrumAnalyzer.#DEFAULTS.minFrequency;
  }

  static get DEFAULT_MAX_FREQUENCY() {
    return SpectrumAnalyzer.#DEFAULTS.maxFrequency;
  }

  constructor({
    layout,
    fftSize = SpectrumAnalyzer.DEFAULT_FFT_SIZE,
    minFrequency = SpectrumAnalyzer.DEFAULT_MIN_FREQUENCY,
    maxFrequency = SpectrumAnalyzer.DEFAULT_MAX_FREQUENCY,
  } = {}) {
    if (!layout) throw new TypeError('SpectrumAnalyzer requires a layout');

    this.layout = layout;
    this.fftSize = fftSize;
    this.minFrequency = minFrequency;
    this.maxFrequency = maxFrequency;

    this.bandEdges = new Array(layout.bandCount + 1).fill(0);
    this.levels = new Array(layout.bandCount).fill(0);
    this.peaks = new Array(layout.bandCount).fill(0);
    this.peakVelocity = new Array(layout.bandCount).fill(0);
    this.labels = new Array(layout.bandCount).fill('');
  }

  attach(node, sampleRate) {
    this.node = node;
    this.bins = new Uint8Array(node.frequencyBinCount);
    this.buildBandEdges(sampleRate);
  }

  buildBandEdges(sampleRate) {
    const binCount = this.fftSize / 2;
    const hzPerBin = (sampleRate / 2) / binCount;
    const frequencyRatio = this.maxFrequency / this.minFrequency;

    for (let i = 0; i <= this.layout.bandCount; i++) {
      const frequency = this.minFrequency
        * frequencyRatio ** (i / this.layout.bandCount);
      this.bandEdges[i] = Math.round(frequency / hzPerBin);

      if (i < this.layout.bandCount) {
        const centerFrequency = this.minFrequency
          * frequencyRatio ** ((i + 0.5) / this.layout.bandCount);
        this.labels[i] = centerFrequency >= 10000
          ? `${Math.round(centerFrequency / 1000)}k`
          : centerFrequency >= 1000
            ? `${(centerFrequency / 1000).toFixed(1)}k`
            : Math.round(centerFrequency).toString();
      }
    }
  }

  update(gain, peakHoldEnabled) {
    if (!this.node || !this.bins) return;

    this.node.getByteFrequencyData(this.bins);
    const binCount = this.bins.length;
    const { release, peakAcceleration } = SpectrumAnalyzer.#BALLISTICS;

    for (const band of this.levels.keys()) {
      const lowBin = this.bandEdges[band];
      const highBin = Math.max(
        Math.min(this.bandEdges[band + 1], binCount),
        lowBin + 1,
      );
      let sum = 0;
      let count = 0;
      for (let bin = lowBin; bin < highBin; bin++, count++) {
        sum += this.bins[bin] / 255;
      }
      const value = Math.min(1, (count ? sum / count : 0) * gain);

      if (value > this.levels[band]) {
        this.levels[band] = value;
      } else {
        this.levels[band] += (value - this.levels[band]) * release;
      }

      if (this.levels[band] >= this.peaks[band]) {
        this.peaks[band] = this.levels[band];
        this.peakVelocity[band] = 0;
      } else {
        this.peakVelocity[band] += peakAcceleration;
        this.peaks[band] = Math.max(
          this.levels[band],
          this.peaks[band] - this.peakVelocity[band],
        );
      }

      if (!peakHoldEnabled) this.peaks[band] = 0;
    }
  }

  reset() {
    this.levels.fill(0);
    this.resetPeaks();
  }

  resetPeaks() {
    this.peaks.fill(0);
    this.peakVelocity.fill(0);
  }

  frameState(peakHoldEnabled) {
    return {
      levels: this.levels,
      peaks: this.peaks,
      labels: this.labels,
      peakHoldEnabled,
    };
  }
}

export default SpectrumAnalyzer;
