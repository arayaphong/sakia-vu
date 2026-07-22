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
  #canvas;
  #layout;
  #context;
  #devicePixelRatio;
  #palette;
  #bandIndices;
  #segmentIndices;

  constructor({
    canvas,
    layout,
    devicePixelRatio = () => globalThis.devicePixelRatio ?? 1,
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

    this.#canvas = canvas;
    this.#layout = layout;
    this.#context = context;
    this.#devicePixelRatio = devicePixelRatio;
    this.#palette = { ...DEFAULT_PALETTE, ...palette };
    this.#bandIndices = Array.from(
      { length: layout.bandCount },
      (_, index) => index,
    );
    this.#segmentIndices = Array.from(
      { length: layout.segmentCount },
      (_, index) => index,
    );
  }

  resize() {
    const reportedRatio = Number(this.#devicePixelRatio());
    const ratio = Number.isFinite(reportedRatio) && reportedRatio > 0
      ? reportedRatio
      : 1;

    this.#canvas.width = Math.round(this.#canvas.clientWidth * ratio);
    this.#canvas.height = Math.round(this.#canvas.clientHeight * ratio);
  }

  render(meterState, physicsObjects = []) {
    const ctx = this.#context;

    ctx.fillStyle = this.#palette.bg;
    ctx.fillRect(0, 0, this.#canvas.width, this.#canvas.height);

    this.#withLogicalTransform(() => this.#drawMeter(meterState));

    if (physicsObjects.length > 0) {
      this.#withLogicalTransform(() => this.#drawPhysicsOverlay(physicsObjects));
    }
  }

  #withLogicalTransform(draw) {
    const ctx = this.#context;
    const { logicalWidth, logicalHeight } = this.#layout;

    ctx.save();
    ctx.scale(
      this.#canvas.width / logicalWidth,
      this.#canvas.height / logicalHeight,
    );
    draw();
    ctx.restore();
  }

  #drawMeter(state) {
    const ctx = this.#context;
    const layout = this.#layout;
    const palette = this.#palette;
    const { levels, peaks, labels, peakHoldEnabled } = state;
    const x0 = layout.padX + (layout.columnWidth - layout.barWidth) / 2;

    this.#bandIndices.forEach((band) => {
      const x = x0 + band * layout.columnWidth;
      const lit = layout.litSegments(levels[band]);
      const peakSegment = layout.litSegments(peaks[band]);

      this.#segmentIndices.forEach((segment) => {
        const ratio = (segment + 1) / layout.segmentCount;
        const y = layout.padTop
          + (layout.segmentCount - 1 - segment)
            * (layout.segmentHeight + layout.segmentGap);
        const isLit = segment < lit;
        const isPeak = peakHoldEnabled
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
      });

      ctx.shadowBlur = 0;
      ctx.globalAlpha = 1;

      ctx.fillStyle = palette.label;
      ctx.font = '22px monospace';
      ctx.textBaseline = 'alphabetic';
      const label = labels[band];
      const textWidth = ctx.measureText(label).width;
      ctx.fillText(
        label,
        x + layout.barWidth / 2 - textWidth / 2,
        layout.logicalHeight - 16,
      );
    });

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
    const ctx = this.#context;

    objects.forEach(({ angle, hue, kind, size, x, y }) => {
      ctx.save();
      ctx.translate(x, y);
      ctx.rotate(angle);
      ctx.globalAlpha = 1;
      ctx.fillStyle = `hsl(${hue}, 80%, 60%)`;

      if (kind === 'ball') {
        ctx.beginPath();
        ctx.arc(0, 0, size, 0, Math.PI * 2);
        ctx.fill();

        ctx.globalAlpha = 0.25;
        ctx.fillStyle = '#fff';
        ctx.beginPath();
        ctx.arc(
          -size * 0.3,
          -size * 0.3,
          size * 0.35,
          0,
          Math.PI * 2,
        );
        ctx.fill();
      } else {
        ctx.beginPath();
        ctx.roundRect(
          -size,
          -size,
          size * 2,
          size * 2,
          6,
        );
        ctx.fill();
      }

      ctx.restore();
    });
  }

  #segmentColor(ratio) {
    if (ratio > 0.82) return this.#palette.red;
    if (ratio > 0.6) return this.#palette.amber;
    return this.#palette.green;
  }
}

export { DEFAULT_PALETTE };
