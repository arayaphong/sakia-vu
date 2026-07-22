'use strict';

// ---------------------------------------------------------------------------
// Layout — port of src/core/models/MeterLayout.h (logical canvas 1640x560).
// Shared by the renderer and the physics world so collision geometry can
// never diverge from the drawn pixels.
// ---------------------------------------------------------------------------
const LOGICAL_W = 1640, LOGICAL_H = 560;
const PAD_X = 24, PAD_TOP = 14, PAD_BOTTOM = 46;
const NUM_BANDS = 16, SEGMENTS = 28, GAP_SEG = 4;
const USABLE_W = LOGICAL_W - PAD_X * 2;                    // 1592
const USABLE_H = LOGICAL_H - PAD_TOP - PAD_BOTTOM;         // 500
const SEG_H = (USABLE_H - (SEGMENTS - 1) * GAP_SEG) / SEGMENTS; // 14
const COL_W = USABLE_W / NUM_BANDS;                        // 99.5
const BAR_W = COL_W * 0.62;
const GROUND_Y = LOGICAL_H - PAD_BOTTOM;                   // 514

function barCenterX(b) { return PAD_X + b * COL_W + COL_W / 2; }
function litSegments(level) { return Math.round(level * SEGMENTS); }
function barTopForLevel(level) {
  let lit = litSegments(level);
  if (lit <= 0) return GROUND_Y;
  if (lit > SEGMENTS) lit = SEGMENTS;
  return PAD_TOP + (SEGMENTS - lit) * (SEG_H + GAP_SEG);
}

// ---------------------------------------------------------------------------
// Spectrum analyzer — Web Audio AnalyserNode. The native C++ analyzer
// (src/audio/FftwSpectrumAnalyzer.cpp) replicates exactly this behaviour:
// FFT 4096, Blackman window, smoothing 0.6, dB range -100..-30, byte mapping.
// ---------------------------------------------------------------------------
const FFT_SIZE = 4096;
const FMIN = 30, FMAX = 16000;

const analyzer = {
  node: null,
  bins: null,
  bandEdges: new Array(NUM_BANDS + 1).fill(0),
  levels: new Array(NUM_BANDS).fill(0),
  peaks: new Array(NUM_BANDS).fill(0),
  peakVel: new Array(NUM_BANDS).fill(0),
  labels: new Array(NUM_BANDS).fill(''),

  attach(node, sampleRate) {
    this.node = node;
    this.bins = new Uint8Array(node.frequencyBinCount);
    this.buildBandEdges(sampleRate);
  },

  buildBandEdges(sampleRate) {
    const binCount = FFT_SIZE / 2;
    const hzPerBin = (sampleRate / 2) / binCount;
    for (let i = 0; i <= NUM_BANDS; i++) {
      const f = FMIN * Math.pow(FMAX / FMIN, i / NUM_BANDS);
      this.bandEdges[i] = Math.round(f / hzPerBin);
      if (i < NUM_BANDS) {
        const fc = FMIN * Math.pow(FMAX / FMIN, (i + 0.5) / NUM_BANDS);
        this.labels[i] = fc >= 10000 ? Math.round(fc / 1000) + 'k'
                       : fc >= 1000  ? (fc / 1000).toFixed(1) + 'k'
                       :               Math.round(fc).toString();
      }
    }
  },

  // One frame of ballistics, matching the C++ per-frame constants (60 fps rAF).
  update(gain, peakHold) {
    if (!this.node) return;
    this.node.getByteFrequencyData(this.bins);
    const binCount = this.bins.length;
    for (let b = 0; b < NUM_BANDS; b++) {
      const lo = this.bandEdges[b];
      const hi = Math.max(Math.min(this.bandEdges[b + 1], binCount), lo + 1);
      let sum = 0, n = 0;
      for (let k = lo; k < hi; k++, n++) sum += this.bins[k] / 255;
      const v = Math.min(1, (n ? sum / n : 0) * gain);

      // Fast attack, slow release.
      if (v > this.levels[b]) this.levels[b] = v;
      else this.levels[b] += (v - this.levels[b]) * 0.22;

      // Peak hold falls under gravity.
      if (this.levels[b] >= this.peaks[b]) {
        this.peaks[b] = this.levels[b];
        this.peakVel[b] = 0;
      } else {
        this.peakVel[b] += 0.0009;
        this.peaks[b] = Math.max(this.levels[b], this.peaks[b] - this.peakVel[b]);
      }
      if (!peakHold) this.peaks[b] = 0;
    }
  },

  reset() {
    this.levels.fill(0);
    this.resetPeaks();
  },

  resetPeaks() {
    this.peaks.fill(0);
    this.peakVel.fill(0);
  },
};

// ---------------------------------------------------------------------------
// Renderer — port of src/ui/SkiaMeterRenderer.cpp (Canvas 2D).
// ---------------------------------------------------------------------------
const COLORS = { green: '#34e07a', amber: '#ffc24b', red: '#ff4d52',
                 unlit: '#161b20', label: '#6b7682', unit: '#3a424c',
                 bg: '#070a0d' };

function segColor(ratio) {
  if (ratio > 0.82) return COLORS.red;
  if (ratio > 0.6) return COLORS.amber;
  return COLORS.green;
}

const canvas = document.getElementById('meter');
const ctx = canvas.getContext('2d');

function resizeCanvas() {
  const dpr = window.devicePixelRatio || 1;
  const w = canvas.clientWidth, h = canvas.clientHeight;
  canvas.width = Math.round(w * dpr);
  canvas.height = Math.round(h * dpr);
}
window.addEventListener('resize', resizeCanvas);

function withLogicalTransform(fn) {
  ctx.save();
  ctx.scale(canvas.width / LOGICAL_W, canvas.height / LOGICAL_H);
  fn();
  ctx.restore();
}

function drawMeter(state) {
  ctx.fillStyle = COLORS.bg;
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  withLogicalTransform(() => {
    const x0 = PAD_X + (COL_W - BAR_W) / 2;
    for (let b = 0; b < NUM_BANDS; b++) {
      const x = x0 + b * COL_W;
      const lit = litSegments(state.levels[b]);
      const peakSeg = litSegments(state.peaks[b]);

      for (let s = 0; s < SEGMENTS; s++) {
        const ratio = (s + 1) / SEGMENTS;
        const y = PAD_TOP + (SEGMENTS - 1 - s) * (SEG_H + GAP_SEG);
        const isLit = s < lit;
        const isPeak = state.peakHoldEnabled && s === peakSeg - 1 && peakSeg > 0;

        ctx.beginPath();
        ctx.roundRect(x, y, BAR_W, SEG_H, 2);
        if (isLit || isPeak) {
          const c = segColor(ratio);
          // Native shadowBlur: this is what the Skia code emulates with a
          // blurred under-layer (8 normal / 14 peak).
          ctx.shadowColor = c;
          ctx.shadowBlur = isPeak ? 14 : 8;
          ctx.fillStyle = c;
          ctx.globalAlpha = isPeak ? 1.0 : 0.95;
        } else {
          ctx.shadowBlur = 0;
          ctx.fillStyle = COLORS.unlit;
          ctx.globalAlpha = 1.0;
        }
        ctx.fill();
      }
      ctx.shadowBlur = 0;
      ctx.globalAlpha = 1.0;

      // Frequency label under the bar.
      ctx.fillStyle = COLORS.label;
      ctx.font = '22px monospace';
      ctx.textBaseline = 'alphabetic';
      const tw = ctx.measureText(state.labels[b]).width;
      ctx.fillText(state.labels[b], x + BAR_W / 2 - tw / 2, LOGICAL_H - 16);
    }

    // "Hz" unit, bottom right.
    ctx.fillStyle = COLORS.unit;
    ctx.font = '20px monospace';
    const tw = ctx.measureText('Hz').width;
    ctx.fillText('Hz', LOGICAL_W - PAD_X - tw, LOGICAL_H - 16);
  });
}

// Port of SkiaMeterRenderer::drawPhysicsOverlay.
function drawPhysicsOverlay(objects) {
  if (!objects.length) return;
  withLogicalTransform(() => {
    for (const o of objects) {
      ctx.save();
      ctx.translate(o.x, o.y);
      ctx.rotate(o.angle);
      ctx.globalAlpha = 1.0;
      ctx.fillStyle = `hsl(${o.hue}, 80%, 60%)`;
      if (o.kind === 'ball') {
        ctx.beginPath();
        ctx.arc(0, 0, o.size, 0, Math.PI * 2);
        ctx.fill();
        ctx.globalAlpha = 0.25;
        ctx.fillStyle = '#fff';
        ctx.beginPath();
        ctx.arc(-o.size * 0.3, -o.size * 0.3, o.size * 0.35, 0, Math.PI * 2);
        ctx.fill();
      } else {
        ctx.beginPath();
        ctx.roundRect(-o.size, -o.size, o.size * 2, o.size * 2, 6);
        ctx.fill();
      }
      ctx.restore();
    }
  });
}

// ---------------------------------------------------------------------------
// Physics world — matter-js port of src/physics/Box2dPhysicsWorld.cpp.
// matter-js is pixel-based, so MeterLayout values are used directly (the C++
// world converts px<->m at 100 px/m; no conversion needed here). The C++
// design itself came from a Matter demo's "reposition + velocity" trick, so
// this is a port back. Anti-trap invariants (docs/PHYSICS.md) are preserved:
// solid gap fillers, one-way peak ledges, per-step enforceSurface() backstop.
// ---------------------------------------------------------------------------
const FIXED_DT = 1 / 60;          // s — one fixed step per 60 Hz frame
const MAX_FRAME_DT = 0.05;        // s — clamp like the C++ accumulator
const MAX_OBJECTS = 16;
const BAR_HALF_H = 300;           // px (C++: 3 m) — bottom never clears ground
const MAX_KINEMATIC_STEP = 50;    // px/step (C++: 30 m/s at 100 px/m, 60 Hz)
const SURFACE_SLOP_PX = 2;        // contact-slop allowance before lifting
const GAP_COUNT = NUM_BANDS - 1;

const physics = {
  available: typeof Matter !== 'undefined',
  engine: null,
  bars: [],
  peaks: [],       // kinematic ledges, added/removed from the world on toggle
  fillers: [],
  objects: [],     // oldest-first [{body, kind, size, hue}]
  barTops: new Array(NUM_BANDS).fill(GROUND_Y),
  peaksInWorld: false,
  accumulator: 0,

  init() {
    if (!this.available) return;
    const { Engine, Bodies, Body, Composite, Events } = Matter;
    this.engine = Engine.create({ enableSleeping: false });
    this.engine.gravity.y = 2; // "demo ratio 2.0" (low gravity: 0.2)

    // Four static walls just outside the canvas; ground top at GROUND_Y.
    const walls = [
      Bodies.rectangle(LOGICAL_W / 2, GROUND_Y + 50, LOGICAL_W + 200, 100),
      Bodies.rectangle(LOGICAL_W / 2, -50, LOGICAL_W + 200, 100),
      Bodies.rectangle(-50, LOGICAL_H / 2, 100, LOGICAL_H + 200),
      Bodies.rectangle(LOGICAL_W + 50, LOGICAL_H / 2, 100, LOGICAL_H + 200),
    ];
    for (const w of walls) {
      w.isStatic = true;
      w.friction = 0.3;
      w.restitution = 0.72;
      Composite.add(this.engine.world, w);
    }

    for (let b = 0; b < NUM_BANDS; b++) {
      const cx = barCenterX(b);
      const bar = Bodies.rectangle(cx, GROUND_Y + BAR_HALF_H, BAR_W, BAR_HALF_H * 2,
        { isStatic: true, friction: 0.1, restitution: 0 });
      this.bars.push(bar);
      Composite.add(this.engine.world, bar);

      // Peak-hold ledge: starts out of the world (peak hold starts off).
      const peak = Bodies.rectangle(cx, GROUND_Y + SEG_H / 2, BAR_W, SEG_H,
        { isStatic: true, friction: 0.2, restitution: 0.3 });
      peak.plugin.isPeakLedge = true;
      this.peaks.push(peak);
    }

    // Solid gap fillers: reshaped every step to fill each slot up to the
    // shorter neighbor's top, so the space below the surface is always solid.
    for (let g = 0; g < GAP_COUNT; g++) {
      const filler = Bodies.rectangle(barCenterX(g) + COL_W / 2, GROUND_Y + 50,
        COL_W - BAR_W, 100, { isStatic: true, friction: 0.08, restitution: 0 });
      this.fillers.push(filler);
      Composite.add(this.engine.world, filler);
    }

    // One-way peak ledges: solid only for objects landing from above.
    Events.on(this.engine, 'beforeUpdate', () => this.filterPeakPairs());
  },

  // Keep a ledge contact only when the object is above the ledge (y-down).
  // Mirrors peakOneWayPreSolve: a ledge descending onto an object standing on
  // a bar is an unsolvable kinematic squeeze, so it must pass through.
  filterPeakPairs() {
    const pairs = this.engine.pairs.list;
    for (const pair of pairs) {
      const aIsPeak = pair.bodyA.plugin.isPeakLedge;
      const bIsPeak = pair.bodyB.plugin.isPeakLedge;
      if (!aIsPeak && !bIsPeak) continue;
      const ledge = aIsPeak ? pair.bodyA : pair.bodyB;
      const obj = aIsPeak ? pair.bodyB : pair.bodyA;
      if (obj.position.y > ledge.position.y) pair.isActive = false;
    }
  },

  step(dtSeconds, meter) {
    if (!this.available) return;
    this.accumulator += Math.min(dtSeconds, MAX_FRAME_DT);
    while (this.accumulator >= FIXED_DT) {
      this.syncKinematics(meter);
      Matter.Engine.update(this.engine, FIXED_DT * 1000);
      this.enforceSurface();
      this.accumulator -= FIXED_DT;
    }
    this.pruneEscaped();
  },

  syncKinematics(meter) {
    const { Body, Composite } = Matter;

    if (meter.peakHoldEnabled !== this.peaksInWorld) {
      this.peaksInWorld = meter.peakHoldEnabled;
      for (let b = 0; b < NUM_BANDS; b++) {
        if (this.peaksInWorld) {
          // Snap into place before enabling so the teleport doesn't launch.
          Body.setPosition(this.peaks[b],
            { x: barCenterX(b), y: barTopForLevel(meter.peaks[b]) + SEG_H / 2 });
          Body.setVelocity(this.peaks[b], { x: 0, y: 0 });
          Composite.add(this.engine.world, this.peaks[b]);
        } else {
          Composite.remove(this.engine.world, this.peaks[b]);
        }
      }
    }

    for (let b = 0; b < NUM_BANDS; b++) {
      // Kinematic drive: reposition + velocity. v = error lands the body
      // exactly on target; the stored velocity imparts momentum to objects.
      const bar = this.bars[b];
      const targetY = barTopForLevel(meter.levels[b]) + BAR_HALF_H;
      const step = Math.max(-MAX_KINEMATIC_STEP,
                    Math.min(MAX_KINEMATIC_STEP, targetY - bar.position.y));
      Body.setPosition(bar, { x: bar.position.x, y: bar.position.y + step });
      Body.setVelocity(bar, { x: 0, y: step });
      this.barTops[b] = bar.position.y - BAR_HALF_H;

      if (this.peaksInWorld) {
        const peak = this.peaks[b];
        const pTarget = barTopForLevel(meter.peaks[b]) + SEG_H / 2;
        const pStep = Math.max(-MAX_KINEMATIC_STEP,
                      Math.min(MAX_KINEMATIC_STEP, pTarget - peak.position.y));
        Body.setPosition(peak, { x: peak.position.x, y: peak.position.y + pStep });
        Body.setVelocity(peak, { x: 0, y: pStep });
      }
    }

    this.syncGapFillers();
  },

  // Flat top at the shorter neighbor's level (y-down: larger y), bottom edge
  // well below the ground line so the filler never degenerates.
  syncGapFillers() {
    for (let g = 0; g < GAP_COUNT; g++) {
      const leftX = barCenterX(g) + BAR_W / 2;
      const rightX = barCenterX(g + 1) - BAR_W / 2;
      const topY = Math.max(this.barTops[g], this.barTops[g + 1]);
      const bottomY = GROUND_Y + 100;
      Matter.Body.setVertices(this.fillers[g], Matter.Vertices.create([
        { x: leftX, y: topY },
        { x: rightX, y: topY },
        { x: rightX, y: bottomY },
        { x: leftX, y: bottomY },
      ], this.fillers[g]));
    }
  },

  // Meter surface y at lx from the bars' actual positions; null outside.
  surfaceYAt(lx) {
    for (let b = 0; b < NUM_BANDS; b++) {
      if (Math.abs(barCenterX(b) - lx) <= BAR_W / 2) return this.barTops[b];
    }
    for (let g = 0; g < GAP_COUNT; g++) {
      const leftX = barCenterX(g) + BAR_W / 2;
      const rightX = barCenterX(g + 1) - BAR_W / 2;
      if (lx > leftX && lx < rightX) {
        return Math.max(this.barTops[g], this.barTops[g + 1]);
      }
    }
    return null;
  },

  // Hard guarantee: lift any object whose lowest point ended a step below the
  // surface back onto it, keeping horizontal motion.
  enforceSurface() {
    const { Body } = Matter;
    for (const o of this.objects) {
      const p = o.body.position;
      let low = { x: p.x, y: p.y + o.size };
      if (o.kind === 'box') {
        // Lowest rotated corner.
        const c = Math.cos(o.body.angle), s = Math.sin(o.body.angle);
        let maxY = -Infinity, cornerX = p.x;
        for (const sx of [-o.size, o.size]) {
          for (const sy of [-o.size, o.size]) {
            const y = p.y + sx * s + sy * c;
            if (y > maxY) { maxY = y; cornerX = p.x + sx * c - sy * s; }
          }
        }
        low = { x: cornerX, y: maxY };
      }

      const surfaceY = this.surfaceYAt(low.x);
      if (surfaceY === null) continue;
      const depth = low.y - surfaceY;
      if (depth <= SURFACE_SLOP_PX) continue;

      Body.setPosition(o.body, { x: p.x, y: p.y - depth });
      const v = o.body.velocity;
      if (v.y > 0) Body.setVelocity(o.body, { x: v.x, y: 0 });
    }
  },

  // Lowest bar-top y among bands overlapping [lx-half, lx+half].
  safeSpawnY(lx, half, ly) {
    let topY = GROUND_Y;
    for (let b = 0; b < NUM_BANDS; b++) {
      if (Math.abs(barCenterX(b) - lx) < half + BAR_W / 2) {
        topY = Math.min(topY, this.barTops[b]);
      }
    }
    return Math.min(ly, topY - half - 6);
  },

  addObject(kind, lx, ly) {
    if (!this.available) return;
    const { Bodies, Composite } = Matter;
    const rand = (a, b) => a + Math.random() * (b - a);

    if (this.objects.length >= MAX_OBJECTS) {
      Composite.remove(this.engine.world, this.objects[0].body);
      this.objects.shift();
    }

    const isBall = kind === 'ball';
    const size = isBall ? rand(42, 60) : rand(40, 55);
    const hue = isBall ? rand(200, 255) : rand(265, 310);

    if (lx < 0) lx = rand(PAD_X + size + 2, LOGICAL_W - PAD_X - size - 2);
    lx = Math.max(size + 2, Math.min(LOGICAL_W - size - 2, lx));
    ly = this.safeSpawnY(lx, size, ly);

    const body = isBall
      ? Bodies.circle(lx, ly, size,
          { friction: 0.05, frictionAir: 0.01, restitution: 0.3 })
      : Bodies.rectangle(lx, ly, size * 2, size * 2,
          { friction: 0.1, frictionAir: 0.005, restitution: 0.2 });
    Composite.add(this.engine.world, body);
    this.objects.push({ body, kind, size, hue });
  },

  spawnBall(lx, ly) { this.addObject('ball', lx, ly); },
  spawnBox(lx, ly) { this.addObject('box', lx, ly); },

  clear() {
    if (!this.available) return;
    for (const o of this.objects) Matter.Composite.remove(this.engine.world, o.body);
    this.objects = [];
  },

  setLowGravity(low) {
    if (this.available) this.engine.gravity.y = low ? 0.2 : 2;
  },

  pruneEscaped() {
    const before = this.objects.length;
    this.objects = this.objects.filter(o => {
      const p = o.body.position;
      if (Math.abs(p.x) > 5000 || Math.abs(p.y) > 5000) {
        Matter.Composite.remove(this.engine.world, o.body);
        return false;
      }
      return true;
    });
    return before !== this.objects.length;
  },

  state() {
    return this.objects.map(o => ({
      kind: o.kind, size: o.size, hue: o.hue,
      x: o.body.position.x, y: o.body.position.y, angle: o.body.angle,
    }));
  },
};
physics.init();

// ---------------------------------------------------------------------------
// Audio source management — Mic (getUserMedia) or tab audio (getDisplayMedia).
// ---------------------------------------------------------------------------
const audio = {
  ctx: null,
  stream: null,
  source: null,
  running: false,

  async start(mode, deviceId) {
    this.stopTracks();
    if (!this.ctx) this.ctx = new AudioContext();
    if (this.ctx.state === 'suspended') await this.ctx.resume();

    if (mode === 'tab') {
      this.stream = await navigator.mediaDevices.getDisplayMedia({
        video: true, audio: true,
      });
      // The video track only carries the picker UI requirement; drop it.
      for (const t of this.stream.getVideoTracks()) t.stop();
      if (!this.stream.getAudioTracks().length) {
        throw new Error('no audio shared with the tab/screen');
      }
    } else {
      this.stream = await navigator.mediaDevices.getUserMedia({
        audio: {
          deviceId: deviceId ? { exact: deviceId } : undefined,
          echoCancellation: false,
          noiseSuppression: false,
          autoGainControl: false,
        },
      });
    }

    if (!this.analyserNode) {
      this.analyserNode = this.ctx.createAnalyser();
      this.analyserNode.fftSize = FFT_SIZE;
      this.analyserNode.smoothingTimeConstant = 0.6;
      this.analyserNode.minDecibels = -100;
      this.analyserNode.maxDecibels = -30;
    }
    this.source = this.ctx.createMediaStreamSource(this.stream);
    this.source.connect(this.analyserNode); // never to destination (feedback)
    analyzer.attach(this.analyserNode, this.ctx.sampleRate);
    this.running = true;

    // Tab sharing ended from the browser UI -> treat as Stop.
    this.stream.getAudioTracks()[0].addEventListener('ended', () => {
      if (this.running) stopCapture();
    });
  },

  stopTracks() {
    if (this.source) { try { this.source.disconnect(); } catch (e) {} this.source = null; }
    if (this.stream) { for (const t of this.stream.getTracks()) t.stop(); this.stream = null; }
    this.running = false;
  },
};

// ---------------------------------------------------------------------------
// UI wiring — mirrors src/app/AppController.cpp.
// ---------------------------------------------------------------------------
const els = {
  listen: document.getElementById('listen'),
  mode: document.getElementById('mode'),
  device: document.getElementById('device'),
  gain: document.getElementById('gain'),
  gainVal: document.getElementById('gainVal'),
  peak: document.getElementById('peak'),
  physicsBtn: document.getElementById('physics'),
  physicsControls: document.getElementById('physicsControls'),
  dropBall: document.getElementById('dropBall'),
  dropBox: document.getElementById('dropBox'),
  clearObjs: document.getElementById('clearObjs'),
  lowGrav: document.getElementById('lowGrav'),
  status: document.getElementById('status'),
};

let peakHold = true;
let physicsEnabled = false;
const meterState = {
  levels: analyzer.levels,
  peaks: analyzer.peaks,
  labels: analyzer.labels,
  peakHoldEnabled: true,
};

function setStatus(kind, text) {
  els.status.className = kind;
  els.status.textContent = text;
}

function updateListenButton() {
  els.listen.textContent = audio.running
    ? (els.mode.value === 'tab' ? '⏹ Stop' : '⏹ Stop')
    : '▶ Listen';
}

async function refreshDeviceList() {
  const isTab = els.mode.value === 'tab';
  els.device.disabled = isTab;
  els.device.innerHTML = '';
  if (isTab) {
    els.device.append(new Option('Browser share picker', ''));
    return;
  }
  els.device.append(new Option('Default input', ''));
  try {
    const devices = await navigator.mediaDevices.enumerateDevices();
    for (const d of devices) {
      if (d.kind === 'audioinput') {
        els.device.append(new Option(d.label || 'Microphone', d.deviceId));
      }
    }
  } catch (e) { /* labels need permission; default input still works */ }
}

async function startCapture() {
  try {
    await audio.start(els.mode.value, els.device.value);
    updateListenButton();
    setStatus('live', '● LIVE');
    refreshDeviceList(); // labels become available after mic permission
  } catch (e) {
    audio.stopTracks();
    updateListenButton();
    setStatus('error', '▲ CAPTURE ERROR' + (e && e.message ? ` — ${e.message}` : ''));
  }
}

function stopCapture() {
  audio.stopTracks();
  analyzer.reset();
  updateListenButton();
  setStatus('stopped', '● STOPPED');
}

els.listen.addEventListener('click', () => {
  if (audio.running) stopCapture();
  else startCapture();
});

els.mode.addEventListener('change', () => {
  refreshDeviceList();
  if (audio.running) startCapture(); // restart against the new source
});

els.device.addEventListener('change', () => {
  if (audio.running) startCapture();
});

els.gain.addEventListener('input', () => {
  els.gainVal.textContent = Number(els.gain.value).toFixed(1) + 'x';
});

els.peak.addEventListener('click', () => {
  peakHold = !peakHold;
  els.peak.classList.toggle('active', peakHold);
  if (!peakHold) analyzer.resetPeaks();
});

els.physicsBtn.addEventListener('click', () => {
  physicsEnabled = !physicsEnabled;
  els.physicsBtn.classList.toggle('active', physicsEnabled);
  els.physicsControls.classList.toggle('hidden', !physicsEnabled);
});

els.dropBall.addEventListener('click', () => physics.spawnBall(-1, -1));
els.dropBox.addEventListener('click', () => physics.spawnBox(-1, -1));
els.clearObjs.addEventListener('click', () => physics.clear());
els.lowGrav.addEventListener('click', () => {
  const low = !els.lowGrav.classList.contains('active');
  els.lowGrav.classList.toggle('active', low);
  physics.setLowGravity(low);
});

// Click-to-spawn: left = ball, right = box (logical canvas coords).
function canvasLogicalCoords(e) {
  const r = canvas.getBoundingClientRect();
  return [
    (e.clientX - r.left) / r.width * LOGICAL_W,
    (e.clientY - r.top) / r.height * LOGICAL_H,
  ];
}
canvas.addEventListener('click', e => {
  if (!physicsEnabled) return;
  const [lx, ly] = canvasLogicalCoords(e);
  physics.spawnBall(lx, ly);
});
canvas.addEventListener('contextmenu', e => {
  e.preventDefault();
  if (!physicsEnabled) return;
  const [lx, ly] = canvasLogicalCoords(e);
  physics.spawnBox(lx, ly);
});

if (!physics.available) {
  els.physicsBtn.disabled = true;
  els.physicsBtn.title = 'matter-js failed to load (CDN unreachable)';
}

// ---------------------------------------------------------------------------
// Frame loop — mirrors AppController::onTick.
// ---------------------------------------------------------------------------
let lastFrameMs = 0;
function frame(nowMs) {
  const dt = lastFrameMs ? (nowMs - lastFrameMs) / 1000 : FIXED_DT;
  lastFrameMs = nowMs;

  if (audio.running) {
    analyzer.update(Number(els.gain.value), peakHold);
  }
  meterState.peakHoldEnabled = peakHold;

  if (physicsEnabled) {
    // Physics uses real frame time (fixed-stepped inside), unlike the
    // analyzer ballistics which assume ~60 fps.
    physics.step(dt, meterState);
  }

  drawMeter(meterState);
  if (physicsEnabled) drawPhysicsOverlay(physics.state());

  requestAnimationFrame(frame);
}

resizeCanvas();
analyzer.buildBandEdges(48000); // labels before the first AudioContext exists
refreshDeviceList();
requestAnimationFrame(frame);
