const DEFAULTS = Object.freeze({
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

/**
 * Immutable geometry shared by the meter renderer and physics world.
 */
export class MeterLayout {
  constructor(options = {}) {
    this.logicalWidth = options.logicalWidth ?? DEFAULTS.logicalWidth;
    this.logicalHeight = options.logicalHeight ?? DEFAULTS.logicalHeight;
    this.padX = options.padX ?? DEFAULTS.padX;
    this.padTop = options.padTop ?? DEFAULTS.padTop;
    this.padBottom = options.padBottom ?? DEFAULTS.padBottom;
    this.bandCount = options.bandCount ?? DEFAULTS.bandCount;
    this.segmentCount = options.segmentCount ?? DEFAULTS.segmentCount;
    this.segmentGap = options.segmentGap ?? DEFAULTS.segmentGap;

    const barWidthRatio = options.barWidthRatio ?? DEFAULTS.barWidthRatio;
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
    let lit = this.litSegments(level);
    if (lit <= 0) return this.groundY;
    if (lit > this.segmentCount) lit = this.segmentCount;
    return this.padTop
      + (this.segmentCount - lit) * (this.segmentHeight + this.segmentGap);
  }
}

export default MeterLayout;
