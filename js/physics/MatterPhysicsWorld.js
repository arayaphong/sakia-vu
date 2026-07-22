'use strict';

const FIXED_DT = 1 / 60;
const MAX_FRAME_DT = 0.05;
const MAX_OBJECTS = 16;
const BAR_HALF_HEIGHT = 300;
const MAX_KINEMATIC_STEP = 50;
const SURFACE_SLOP_PX = 2;

/**
 * Matter.js adapter for the meter's dynamic collision surface.
 *
 * Matter is injected so this module remains importable when the CDN is
 * unavailable and can be tested without relying on browser globals.
 */
export class MatterPhysicsWorld {
  constructor({ Matter, layout, random = Math.random }) {
    if (!layout) {
      throw new TypeError('MatterPhysicsWorld requires a meter layout');
    }

    this.Matter = Matter;
    this.layout = layout;
    this.random = random;
    this.available = Boolean(Matter);

    this.engine = null;
    this.bars = [];
    this.peaks = [];
    this.fillers = [];
    this.objects = [];
    this.barTops = new Array(layout.bandCount).fill(layout.groundY);
    this.peaksInWorld = false;
    this.accumulator = 0;

    if (this.available) this.#initialize();
  }

  step(dtSeconds, meter) {
    if (!this.available) return;

    this.accumulator += Math.min(dtSeconds, MAX_FRAME_DT);
    while (this.accumulator >= FIXED_DT) {
      this.#syncKinematics(meter);
      this.Matter.Engine.update(this.engine, FIXED_DT * 1000);
      this.#enforceSurface();
      this.accumulator -= FIXED_DT;
    }

    this.#pruneEscaped();
  }

  spawnBall(logicalX, logicalY) {
    this.#addObject('ball', logicalX, logicalY);
  }

  spawnBox(logicalX, logicalY) {
    this.#addObject('box', logicalX, logicalY);
  }

  clear() {
    if (!this.available) return;

    for (const object of this.objects) {
      this.Matter.Composite.remove(this.engine.world, object.body);
    }
    this.objects = [];
  }

  setLowGravity(lowGravity) {
    if (!this.available) return;
    this.engine.gravity.y = lowGravity ? 0.2 : 2;
  }

  // Meter surface y at logicalX from the bars' actual positions.
  surfaceYAt(logicalX) {
    const { bandCount, barWidth, columnWidth } = this.layout;

    for (let band = 0; band < bandCount; band += 1) {
      if (Math.abs(this.layout.barCenterX(band) - logicalX) <= barWidth / 2) {
        return this.barTops[band];
      }
    }

    for (let gap = 0; gap < bandCount - 1; gap += 1) {
      const leftX = this.layout.barCenterX(gap) + barWidth / 2;
      const rightX = this.layout.barCenterX(gap + 1) - barWidth / 2;
      if (logicalX > leftX && logicalX < rightX) {
        return Math.max(this.barTops[gap], this.barTops[gap + 1]);
      }
    }

    return null;
  }

  // Highest safe spawn position over every overlapping meter column.
  safeSpawnY(logicalX, halfSize, logicalY) {
    const { bandCount, barWidth, groundY } = this.layout;
    let topY = groundY;

    for (let band = 0; band < bandCount; band += 1) {
      if (
        Math.abs(this.layout.barCenterX(band) - logicalX)
        < halfSize + barWidth / 2
      ) {
        topY = Math.min(topY, this.barTops[band]);
      }
    }

    return Math.min(logicalY, topY - halfSize - 6);
  }

  snapshot() {
    if (!this.available) return [];

    return this.objects.map((object) => ({
      kind: object.kind,
      size: object.size,
      hue: object.hue,
      x: object.body.position.x,
      y: object.body.position.y,
      angle: object.body.angle,
    }));
  }

  #initialize() {
    const { Engine, Bodies, Composite, Events } = this.Matter;
    const {
      logicalWidth,
      logicalHeight,
      groundY,
      bandCount,
      barWidth,
      segmentHeight,
      columnWidth,
    } = this.layout;

    this.engine = Engine.create({ enableSleeping: false });
    this.engine.gravity.y = 2;

    const walls = [
      Bodies.rectangle(logicalWidth / 2, groundY + 50, logicalWidth + 200, 100),
      Bodies.rectangle(logicalWidth / 2, -50, logicalWidth + 200, 100),
      Bodies.rectangle(-50, logicalHeight / 2, 100, logicalHeight + 200),
      Bodies.rectangle(logicalWidth + 50, logicalHeight / 2, 100, logicalHeight + 200),
    ];

    for (const wall of walls) {
      wall.isStatic = true;
      wall.friction = 0.3;
      wall.restitution = 0.72;
      Composite.add(this.engine.world, wall);
    }

    for (let band = 0; band < bandCount; band += 1) {
      const centerX = this.layout.barCenterX(band);
      const bar = Bodies.rectangle(
        centerX,
        groundY + BAR_HALF_HEIGHT,
        barWidth,
        BAR_HALF_HEIGHT * 2,
        { isStatic: true, friction: 0.1, restitution: 0 },
      );
      this.bars.push(bar);
      Composite.add(this.engine.world, bar);

      const peak = Bodies.rectangle(
        centerX,
        groundY + segmentHeight / 2,
        barWidth,
        segmentHeight,
        { isStatic: true, friction: 0.2, restitution: 0.3 },
      );
      peak.plugin.isPeakLedge = true;
      this.peaks.push(peak);
    }

    for (let gap = 0; gap < bandCount - 1; gap += 1) {
      const filler = Bodies.rectangle(
        this.layout.barCenterX(gap) + columnWidth / 2,
        groundY + 50,
        columnWidth - barWidth,
        100,
        { isStatic: true, friction: 0.08, restitution: 0 },
      );
      this.fillers.push(filler);
      Composite.add(this.engine.world, filler);
    }

    Events.on(this.engine, 'beforeUpdate', () => this.#filterPeakPairs());
  }

  #filterPeakPairs() {
    for (const pair of this.engine.pairs.list) {
      const bodyAIsPeak = pair.bodyA.plugin.isPeakLedge;
      const bodyBIsPeak = pair.bodyB.plugin.isPeakLedge;
      if (!bodyAIsPeak && !bodyBIsPeak) continue;

      const ledge = bodyAIsPeak ? pair.bodyA : pair.bodyB;
      const object = bodyAIsPeak ? pair.bodyB : pair.bodyA;
      if (object.position.y > ledge.position.y) pair.isActive = false;
    }
  }

  #syncKinematics(meter) {
    const { Body, Composite } = this.Matter;
    const { bandCount, segmentHeight } = this.layout;

    if (meter.peakHoldEnabled !== this.peaksInWorld) {
      this.peaksInWorld = meter.peakHoldEnabled;
      for (let band = 0; band < bandCount; band += 1) {
        if (this.peaksInWorld) {
          Body.setPosition(this.peaks[band], {
            x: this.layout.barCenterX(band),
            y: this.layout.barTopForLevel(meter.peaks[band]) + segmentHeight / 2,
          });
          Body.setVelocity(this.peaks[band], { x: 0, y: 0 });
          Composite.add(this.engine.world, this.peaks[band]);
        } else {
          Composite.remove(this.engine.world, this.peaks[band]);
        }
      }
    }

    for (let band = 0; band < bandCount; band += 1) {
      const bar = this.bars[band];
      const targetY = this.layout.barTopForLevel(meter.levels[band])
        + BAR_HALF_HEIGHT;
      const step = Math.max(
        -MAX_KINEMATIC_STEP,
        Math.min(MAX_KINEMATIC_STEP, targetY - bar.position.y),
      );
      Body.setPosition(bar, {
        x: bar.position.x,
        y: bar.position.y + step,
      });
      Body.setVelocity(bar, { x: 0, y: step });
      this.barTops[band] = bar.position.y - BAR_HALF_HEIGHT;

      if (this.peaksInWorld) {
        const peak = this.peaks[band];
        const peakTarget = this.layout.barTopForLevel(meter.peaks[band])
          + segmentHeight / 2;
        const peakStep = Math.max(
          -MAX_KINEMATIC_STEP,
          Math.min(MAX_KINEMATIC_STEP, peakTarget - peak.position.y),
        );
        Body.setPosition(peak, {
          x: peak.position.x,
          y: peak.position.y + peakStep,
        });
        Body.setVelocity(peak, { x: 0, y: peakStep });
      }
    }

    this.#syncGapFillers();
  }

  #syncGapFillers() {
    const { Body, Vertices } = this.Matter;
    const { bandCount, barWidth, groundY } = this.layout;

    for (let gap = 0; gap < bandCount - 1; gap += 1) {
      const leftX = this.layout.barCenterX(gap) + barWidth / 2;
      const rightX = this.layout.barCenterX(gap + 1) - barWidth / 2;
      const topY = Math.max(this.barTops[gap], this.barTops[gap + 1]);
      const bottomY = groundY + 100;

      Body.setVertices(this.fillers[gap], Vertices.create([
        { x: leftX, y: topY },
        { x: rightX, y: topY },
        { x: rightX, y: bottomY },
        { x: leftX, y: bottomY },
      ], this.fillers[gap]));
    }
  }

  #enforceSurface() {
    const { Body } = this.Matter;

    for (const object of this.objects) {
      const position = object.body.position;
      let lowestPoint = { x: position.x, y: position.y + object.size };

      if (object.kind === 'box') {
        const cosine = Math.cos(object.body.angle);
        const sine = Math.sin(object.body.angle);
        let maximumY = -Infinity;
        let cornerX = position.x;

        for (const signX of [-object.size, object.size]) {
          for (const signY of [-object.size, object.size]) {
            const y = position.y + signX * sine + signY * cosine;
            if (y > maximumY) {
              maximumY = y;
              cornerX = position.x + signX * cosine - signY * sine;
            }
          }
        }

        lowestPoint = { x: cornerX, y: maximumY };
      }

      const surfaceY = this.surfaceYAt(lowestPoint.x);
      if (surfaceY === null) continue;

      const depth = lowestPoint.y - surfaceY;
      if (depth <= SURFACE_SLOP_PX) continue;

      Body.setPosition(object.body, {
        x: position.x,
        y: position.y - depth,
      });
      const velocity = object.body.velocity;
      if (velocity.y > 0) {
        Body.setVelocity(object.body, { x: velocity.x, y: 0 });
      }
    }
  }

  #addObject(kind, logicalX, logicalY) {
    if (!this.available) return;

    const { Bodies, Composite } = this.Matter;
    const randomBetween = (minimum, maximum) => (
      minimum + this.random() * (maximum - minimum)
    );

    if (this.objects.length >= MAX_OBJECTS) {
      Composite.remove(this.engine.world, this.objects[0].body);
      this.objects.shift();
    }

    const isBall = kind === 'ball';
    const size = isBall ? randomBetween(42, 60) : randomBetween(40, 55);
    const hue = isBall ? randomBetween(200, 255) : randomBetween(265, 310);

    if (logicalX < 0) {
      logicalX = randomBetween(
        this.layout.padX + size + 2,
        this.layout.logicalWidth - this.layout.padX - size - 2,
      );
    }
    logicalX = Math.max(
      size + 2,
      Math.min(this.layout.logicalWidth - size - 2, logicalX),
    );
    logicalY = this.safeSpawnY(logicalX, size, logicalY);

    const body = isBall
      ? Bodies.circle(logicalX, logicalY, size, {
        friction: 0.05,
        frictionAir: 0.01,
        restitution: 0.3,
      })
      : Bodies.rectangle(logicalX, logicalY, size * 2, size * 2, {
        friction: 0.1,
        frictionAir: 0.005,
        restitution: 0.2,
      });

    Composite.add(this.engine.world, body);
    this.objects.push({ body, kind, size, hue });
  }

  #pruneEscaped() {
    this.objects = this.objects.filter((object) => {
      const { x, y } = object.body.position;
      if (Math.abs(x) > 5000 || Math.abs(y) > 5000) {
        this.Matter.Composite.remove(this.engine.world, object.body);
        return false;
      }
      return true;
    });
  }
}

export const PHYSICS_TIMING = Object.freeze({
  fixedDt: FIXED_DT,
  maxFrameDt: MAX_FRAME_DT,
});
