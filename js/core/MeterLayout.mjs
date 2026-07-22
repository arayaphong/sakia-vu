/**
 * Immutable geometry shared by the meter renderer and physics world.
 */
export class MeterLayout {
  static #DEFAULTS = Object.freeze({
    logicalWidth: 1640,
    logicalHeight: 560,
    padX: 24,
    padTop: 14,
    padBottom: 46,
    bandCount: 16,
    segmentCount: 28,
    segmentGap: 4,
    barWidthRatio: 0.62,
  });

  logicalWidth;
  logicalHeight;
  padX;
  padTop;
  padBottom;
  bandCount;
  segmentCount;
  segmentGap;
  usableWidth;
  usableHeight;
  segmentHeight;
  columnWidth;
  barWidth;
  groundY;

  static get DEFAULTS() {
    return MeterLayout.#DEFAULTS;
  }

  constructor(options = {}) {
    const defaults = MeterLayout.#DEFAULTS;
    this.logicalWidth = options.logicalWidth ?? defaults.logicalWidth;
    this.logicalHeight = options.logicalHeight ?? defaults.logicalHeight;
    this.padX = options.padX ?? defaults.padX;
    this.padTop = options.padTop ?? defaults.padTop;
    this.padBottom = options.padBottom ?? defaults.padBottom;
    this.bandCount = options.bandCount ?? defaults.bandCount;
    this.segmentCount = options.segmentCount ?? defaults.segmentCount;
    this.segmentGap = options.segmentGap ?? defaults.segmentGap;

    const barWidthRatio = options.barWidthRatio ?? defaults.barWidthRatio;
    this.usableWidth = this.logicalWidth - this.padX * 2;
    this.usableHeight = this.logicalHeight - this.padTop - this.padBottom;
    this.segmentHeight = (
      this.usableHeight - (this.segmentCount - 1) * this.segmentGap
    ) / this.segmentCount;
    this.columnWidth = this.usableWidth / this.bandCount;
    this.barWidth = this.columnWidth * barWidthRatio;
    this.groundY = this.logicalHeight - this.padBottom;

    Object.freeze(this);
  }

  barCenterX(index) {
    return this.padX + index * this.columnWidth + this.columnWidth / 2;
  }

  litSegments(level) {
    return Math.round(level * this.segmentCount);
  }

  barTopForLevel(level) {
    const lit = Math.min(
      Math.max(this.litSegments(level), 0),
      this.segmentCount,
    );
    if (lit === 0) return this.groundY;
    return this.padTop
      + (this.segmentCount - lit) * (this.segmentHeight + this.segmentGap);
  }
}

export default MeterLayout;
