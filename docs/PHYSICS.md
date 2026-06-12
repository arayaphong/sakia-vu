# SakiaVU Physics Playground — Collision Design

How `Box2dPhysicsWorld` keeps objects on top of the meter, and the history of the
trap bugs that shaped it. Read this before changing anything in `src/physics/`.

## World overview

- Box2D v3.1.1, 100 px/m, **y-down** (same orientation as the logical canvas, so
  positions/angles map to Skia without sign flips). Fixed 1/60 s stepping with an
  accumulator, 4 sub-steps.
- Bodies:
  - **Walls** — four static boxes just outside the canvas; ground top edge exactly
    on the bar-bottom line (`kGroundY`).
  - **Bars** — one kinematic box per band, 6 m tall so the bottom never clears the
    ground. Driven by per-step velocity targeting (`v = error/dt`, clamped to
    `kMaxKinematicSpeed` = 30 m/s ≈ 50 px/step) so the solver imparts real momentum
    and bullet CCD works.
  - **Peak ledges** — thin kinematic platforms tracking the held peak dots,
    enabled with the Peak Hold toggle. **One-way**: see below.
  - **Gap fillers** — one solid convex quad per gap between neighboring bars,
    from the bar-top line down past the ground. Reshaped each step
    (`gapQuad`/`syncGapFillers`) to the bars' *predicted post-step* tops so the
    surface stays flush with the physical bars mid-spike.
  - **Objects** — dynamic balls/boxes, `isBullet = true`, capped at 16 with
    oldest-first eviction.
- `core/models/MeterLayout.h` holds the shared constexpr layout math so collision
  geometry and drawn pixels can never diverge.

## The trap problem (fixed 2026-06-12)

Objects could end up visually buried — wedged below the gap between two bars, or
embedded inside a bar until it receded. Root causes found, in order:

1. **Thin teleporting geometry.** An early fix bridged the gaps with thin capsules
   repositioned via `b2Shape_SetCapsule`. Rewriting shape geometry is a teleport:
   no sweep, no CCD, no momentum — a fast-rising 3 px surface skips clean past
   objects, leaving them underneath (an earlier `fix-struck-object` branch worked
   around this with a stuck-detector + random relaunch).
2. **Embed-rate vs escape-rate asymmetry.** Replacing the capsules with solid
   convex quads filling each gap slot made the trap state mostly unreachable, but
   a surface rising at up to 50 px/step embeds resting objects faster than the
   solver's clamped depenetration (`maxContactPushSpeed` 3 m/s ≈ 5 px/step) can
   expel them, and the bar side walls (horizontal normals) provide no lift —
   sustained spikes could re-embed objects indefinitely.
3. **Kinematic-vs-kinematic squeeze.** A peak-hold ledge descending onto an object
   standing on a bar (or a bar rising into a held ledge) is unsolvable for any
   solver — both bodies have infinite mass — so the object was pressed into the
   bar until the bar dropped below it.

## The shipped design

Three layered mechanisms in `Box2dPhysicsWorld`:

- **Solid gap fillers** (cause 1) — the space below the visual surface is always
  solid, so a rising surface cannot skip past an object; it overlaps it and the
  solver lifts it out at `maxContactPushSpeed`.
- **One-way peak ledges** (cause 3) — `peakOneWayPreSolve` (Box2D pre-solve
  callback, enabled only on peak shapes) keeps ledges solid only against objects
  landing from above; descending ledges pass through objects instead of crushing
  them. The catch-falling-objects gimmick is preserved.
- **`enforceSurface()` backstop** (cause 2 and anything unforeseen) — after every
  fixed step, any object whose lowest point (ball bottom / lowest rotated box
  corner) sits below the composite meter surface (bar top over a column, lerp
  over a gap, measured at that point's *own* x) is lifted back onto it, keeping
  horizontal velocity. This makes "below the surface across a frame" unreachable
  *by construction*, whatever the cause. Measuring at the lowest point's own x
  matters: measuring at the object's center x false-triggers on slanted fillers
  and makes resting boxes jitter.

Known accepted edge: at full-scale levels the bar tops approach the ceiling and
objects riding them have nowhere to be; the backstop pins them at the bar top,
partially above the visible canvas, until the bars recede.

## Do not regress

- Never add thin collision geometry that moves by shape rewriting (`b2Shape_Set*`
  is a teleport). The gap fillers are deep on purpose.
- Keep kinematic bodies driven by velocity, not `b2Body_SetTransform` (the only
  exception: snapping peak ledges into place on enable, while their velocity is
  zeroed).
- Keep peak ledges one-way, or they crush objects against the bars.
- Keep `enforceSurface()` running after every fixed step — it is the hard
  guarantee that nothing rests below the meter surface.

## Testing

`tools/physics_test.cpp` (`physics-test` target, no GTK/Skia/audio, deterministic
fixed RNG seed) drives the world through four phases — sine ballistics with
peak-hold, gap-center ball drops onto a flat held level, checkerboard square-wave
spikes, and falling peak-ledge sweeps — asserting objects stay finite and in
bounds, never rest below the bar/gap surface line for more than 1.5 s, and that
the object cap and `clear()` hold. Disabling the one-way ledges and the backstop
reproduces the original bug and fails the test; this was verified when they were
introduced.
