'use strict';

const DEFAULT_PALETTE = Object.freeze({
  green: '#34e07a',
  amber: '#ffc24b',
  red: '#ff4d52',
  unlit: '#161b20',
  label: '#6b7682',
  unit: '#3a424c',
  bg: '#070a0d',
});

/**
 * Draws the spectrum meter and its optional physics overlay on a Canvas 2D
 * surface. All geometry comes from the injected MeterLayout instance so the
 * renderer and collision world share one coordinate system.
 */
export class MeterCanvasRenderer {
  constructor({
    canvas,
    layout,
    devicePixelRatio = () => globalThis.devicePixelRatio || 1,
    palette = {},
  }) {
    if (!canvas || typeof canvas.getContext !== 'function') {
      throw new TypeError('MeterCanvasRenderer requires a canvas element');
    }
    if (!layout) {
      throw new TypeError('MeterCanvasRenderer requires a meter layout');
    }

    const context = canvas.getContext('2d');
    if (!context) {
      throw new Error('Canvas 2D rendering is unavailable');
    }

    this.canvas = canvas;
    this.layout = layout;
    this.context = context;
    this.devicePixelRatio = devicePixelRatio;
    this.palette = { ...DEFAULT_PALETTE, ...palette };
  }

  resize() {
    const reportedRatio = Number(this.devicePixelRatio());
    const ratio = Number.isFinite(reportedRatio) && reportedRatio > 0
      ? reportedRatio
      : 1;

    this.canvas.width = Math.round(this.canvas.clientWidth * ratio);
    this.canvas.height = Math.round(this.canvas.clientHeight * ratio);
  }

  render(meterState, physicsObjects = []) {
    const { context: ctx, canvas } = this;

    ctx.fillStyle = this.palette.bg;
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    this.#withLogicalTransform(() => this.#drawMeter(meterState));

    if (physicsObjects.length > 0) {
      this.#withLogicalTransform(() => this.#drawPhysicsOverlay(physicsObjects));
    }
  }

  #withLogicalTransform(draw) {
    const { context: ctx, canvas, layout } = this;

    ctx.save();
    ctx.scale(
      canvas.width / layout.logicalWidth,
      canvas.height / layout.logicalHeight,
    );
    draw();
    ctx.restore();
  }

  #drawMeter(state) {
    const { context: ctx, layout, palette } = this;
    const x0 = layout.padX + (layout.columnWidth - layout.barWidth) / 2;

    for (let band = 0; band < layout.bandCount; band += 1) {
      const x = x0 + band * layout.columnWidth;
      const lit = layout.litSegments(state.levels[band]);
      const peakSegment = layout.litSegments(state.peaks[band]);

      for (let segment = 0; segment < layout.segmentCount; segment += 1) {
        const ratio = (segment + 1) / layout.segmentCount;
        const y = layout.padTop
          + (layout.segmentCount - 1 - segment)
            * (layout.segmentHeight + layout.segmentGap);
        const isLit = segment < lit;
        const isPeak = state.peakHoldEnabled
          && segment === peakSegment - 1
          && peakSegment > 0;

        ctx.beginPath();
        ctx.roundRect(x, y, layout.barWidth, layout.segmentHeight, 2);

        if (isLit || isPeak) {
          const color = this.#segmentColor(ratio);
          ctx.shadowColor = color;
          ctx.shadowBlur = isPeak ? 14 : 8;
          ctx.fillStyle = color;
          ctx.globalAlpha = isPeak ? 1 : 0.95;
        } else {
          ctx.shadowBlur = 0;
          ctx.fillStyle = palette.unlit;
          ctx.globalAlpha = 1;
        }

        ctx.fill();
      }

      ctx.shadowBlur = 0;
      ctx.globalAlpha = 1;

      ctx.fillStyle = palette.label;
      ctx.font = '22px monospace';
      ctx.textBaseline = 'alphabetic';
      const label = state.labels[band];
      const textWidth = ctx.measureText(label).width;
      ctx.fillText(
        label,
        x + layout.barWidth / 2 - textWidth / 2,
        layout.logicalHeight - 16,
      );
    }

    ctx.fillStyle = palette.unit;
    ctx.font = '20px monospace';
    const unitWidth = ctx.measureText('Hz').width;
    ctx.fillText(
      'Hz',
      layout.logicalWidth - layout.padX - unitWidth,
      layout.logicalHeight - 16,
    );
  }

  #drawPhysicsOverlay(objects) {
    const { context: ctx } = this;

    for (const object of objects) {
      ctx.save();
      ctx.translate(object.x, object.y);
      ctx.rotate(object.angle);
      ctx.globalAlpha = 1;
      ctx.fillStyle = `hsl(${object.hue}, 80%, 60%)`;

      if (object.kind === 'ball') {
        ctx.beginPath();
        ctx.arc(0, 0, object.size, 0, Math.PI * 2);
        ctx.fill();

        ctx.globalAlpha = 0.25;
        ctx.fillStyle = '#fff';
        ctx.beginPath();
        ctx.arc(
          -object.size * 0.3,
          -object.size * 0.3,
          object.size * 0.35,
          0,
          Math.PI * 2,
        );
        ctx.fill();
      } else {
        ctx.beginPath();
        ctx.roundRect(
          -object.size,
          -object.size,
          object.size * 2,
          object.size * 2,
          6,
        );
        ctx.fill();
      }

      ctx.restore();
    }
  }

  #segmentColor(ratio) {
    if (ratio > 0.82) return this.palette.red;
    if (ratio > 0.6) return this.palette.amber;
    return this.palette.green;
  }
}

export { DEFAULT_PALETTE };
