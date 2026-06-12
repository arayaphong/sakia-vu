# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

**SakiaVU** — A native 16-band spectrum/VU meter desktop application written in C++ using GTK4 and Skia for rendering.

Planned work and known constraints are tracked in [docs/ROADMAP.md](docs/ROADMAP.md) —
read its "Known constraints" section before touching the analyzer ballistics, font
loading, or pixel-format code.

Code structure and dependency-injection boundaries are documented in
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md). Read it before moving files or adding
new concrete services.

Features:
- 16 frequency bands, logarithmic scale 30 Hz → 16 kHz
- 28 LED segments per band, color-coded green/amber/red by level (0–60% green, 60–82% amber, >82% red)
- Peak hold with gravity-fall animation
- Gain slider (0.1×–1.5×)
- Real-time audio capture via PipeWire (microphone or system output monitor)
- Physics playground overlay (Box2D): balls/boxes launched by the rising bars and
  held peak dots; click canvas = ball, right-click = box. The collision world has
  anti-trap invariants (solid gap fillers, one-way peak ledges, per-step
  `enforceSurface()` backstop) — read [docs/PHYSICS.md](docs/PHYSICS.md) before
  changing `Box2dPhysicsWorld`

## Build System

The project uses **CMake** (target: CMake ≥ 3.25) with **Ninja** as the generator.

```bash
# Configure (first time)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run
./build/sakia-vu

# Offscreen render smoke test (no GTK/mic needed; writes a PNG to inspect)
cmake --build build --target render-test
./build/render-test /tmp/sakia-render.png

# Headless physics smoke test (no GTK/Skia; bounds, eviction, and
# below-surface-trap asserts — see docs/PHYSICS.md)
cmake --build build --target physics-test
./build/physics-test
```

## Dependencies

| Library | Source | Purpose |
|---|---|---|
| GTK 4 | system (pkg-config) | Window, event loop, GtkDrawingArea host |
| Skia m148 | vendored prebuilt in `third_party/skia/` | 2D rendering (segments, glow, labels) |
| PipeWire (libpipewire-0.3) | system | Audio capture |
| FFTW3 (fftw3f) | system | FFT — single-precision float variant |
| Box2D v3.1.1 | CMake FetchContent (pinned tag, needs network on first configure) | Physics playground rigid bodies |

Skia is the prebuilt static `Skia-Linux-Release-x64.zip` from aseprite/skia releases
(tag `m148-a29c8d23be`), unpacked to `third_party/skia/` (not committed — re-download if
missing). It is built against **libstdc++**, so plain g++ links fine. Its bundled deps
(freetype, harfbuzz, png, zlib, expat, jpeg, webp, wuffs, skcms) are linked from
`third_party/skia/out/Release-x64/` inside a `--start-group` block.

**Gotcha:** do NOT use `SkFontMgr_New_FontConfig` from this prebuilt — it hangs for
minutes enumerating family names on hosts with aliasing-heavy fontconfig setups (this
machine has one). `SkiaMeterRenderer` loads a known mono TTF directly via
`SkFontMgr_New_Custom_Empty()->makeFromFile()` instead.

## Architecture

```
src/app               — Composition root and GtkApplication controller; depends on core interfaces
src/core/interfaces   — IAudioSource, ISpectrumAnalyzer, IMeterWidget, IMeterWidgetFactory, IPhysicsWorld
src/core/models       — MeterState, MeterLayout (shared layout constants), PhysicsState
src/audio             — PipeWireAudioCapture and FftwSpectrumAnalyzer implementations
src/physics           — Box2dPhysicsWorld implementation (Box2D v3, 100 px/m, y-down world)
src/ui                — GtkMeterWidget host and SkiaMeterRenderer drawing implementation
tools/render_test.cpp — Offscreen smoke test: synthetic tones → analyzer → PNG
tools/physics_test.cpp — Headless physics smoke test (bounds/eviction/trap asserts)
```

Data flow: `IAudioSource → latest(sampleCount) → ISpectrumAnalyzer → MeterState → IMeterWidget → SkiaMeterRenderer → SkSurface → cairo`

Layer rule: app code should receive abstractions through constructors. Concrete
PipeWire/FFTW/GTK/Skia classes are wired in `src/app/main.cpp`.

Animation runs via `gtk_widget_add_tick_callback` on the meter widget; analyzer ballistics
are per-frame (matching the HTML's rAF loop), updated only while capture is running.

## Rendering Details (matching HTML reference)

- Canvas logical size: 1640 × 560 (scaled to widget size preserving aspect ratio)
- `padX=24, padTop=14, padBottom=46`
- `barW = colW * 0.62`, `gapSeg = 4 px`, segment corner radius = 2 px
- Unlit segment colour: `#161b20`; glow `shadowBlur` emulated with Skia `SkMaskFilter::MakeBlur`
- Peak dot: extra glow blur radius 14 vs 8 for normal lit segments
- Font: monospace 22 px for band labels, 20 px for "Hz" unit

## Signal Processing

```
FFT size: 4096 samples, Blackman window (matches WebAudio AnalyserNode)
Per-bin magnitude smoothing: X̂[k] = 0.6·X̂prev[k] + 0.4·(|fft[k]|/N)   (smoothingTimeConstant)
dB mapping: clamp((20·log10(X̂) + 100) / 70, 0, 1)                      (minDb -100, maxDb -30)
Band value: mean of mapped bin values in range, × gain, clamped to 1
Ballistics: attack = instant (if v > level), release = level += (v - level) * 0.22
Peak fall: peakVel += 0.0009 per frame; peaks[b] -= peakVel[b]
Band edges: f_i = FMIN * (FMAX/FMIN)^(i/NUM_BANDS) converted to FFT bin indices
```
