# SakiaVU — Future Implementation Roadmap

Status of the codebase as of 2026-06-11: v0.1 works end-to-end — PipeWire mono capture,
FFTW 4096-point analysis, Skia CPU raster rendering inside a GtkDrawingArea,
start/stop + gain + peak-hold controls. This document lists planned work, roughly in
priority order, with notes on where each change lands in the current code.

---

## v0.2 — Audio robustness & UX

### 1. Input device selection — done
`AppController` now shows a `GtkDropDown` of PipeWire input sources, with a
`Default input` option that preserves PipeWire autoconnect behavior.

- `IAudioSource` exposes a small `AudioDevice` contract plus selected-target accessors.
- `PipeWireAudioCapture::devices()` enumerates `PW_TYPE_INTERFACE_Node` entries and
  filters `media.class == "Audio/Source"`.
- `PipeWireAudioCapture::start()` passes the selected source through
  `PW_KEY_TARGET_OBJECT`.
- Changing the dropdown while running restarts capture against the new source.

### 2. "What's playing" mode (monitor capture) — done
Meter the system output instead of the mic.

- `IAudioSource` now exposes `AudioCaptureMode`.
- `AppController` has a Mic / Output toggle next to the Listen button.
- Output mode enumerates PipeWire `media.class == "Audio/Sink"` nodes and changes the
  device dropdown to default/output devices.
- `PipeWireAudioCapture::start()` sets `PW_KEY_STREAM_CAPTURE_SINK = "true"` in output
  mode, preserving autoconnect for the default output monitor and using
  `PW_KEY_TARGET_OBJECT` for a selected output device.

### 3. Stereo
The stream currently forces `channels = 1` (PipeWire downmixes). For a proper L/R view:
- Request 2 channels; deinterleave in `PipeWireAudioCapture::onProcess`.
- Two `ISpectrumAnalyzer` instances, or one analyzer implementation extended to N
  channels (preferred: keep its FFT plan shared, duplicate per-channel state arrays).
- Renderer: either mirrored split (L left / R right) or 32 thin bars interleaved.
  Decide when implementing; mirrored split keeps the 16-band look.

### 4. Settings persistence
Persist gain, peak-hold, device choice, window size across runs. Use `GSettings` with a
schema (`dev.arme.SakiaVU.gschema.xml`) — proper for GTK apps but needs schema
compilation at install time; until packaging (v0.4) exists, a plain GKeyFile in
`~/.config/sakiavu.ini` is less friction. Start with GKeyFile, migrate when packaged.

### 5. Error surfacing
`onToggle` shows "▲ CAPTURE ERROR" but PipeWire failures are silent in the stream events. Hook
`pw_stream_events.state_changed`, watch for `PW_STREAM_STATE_ERROR`, forward the error
string to the status label via `g_idle_add`.

---

## v0.3 — Rendering & DSP quality

### 6. GPU rendering (GtkGLArea + Skia Ganesh)
CPU raster at ~1640×560 @60 fps is fine on this machine, but fullscreen/4K won't be.
The vendored prebuilt was built with GL support (`libglvnd` in deps).

- Replace `GtkDrawingArea` with `GtkGLArea`; in `realize`, build a
  `GrDirectContext` via `GrGLMakeNativeInterface()`, wrap the GtkGLArea's FBO with
  `GrBackendRenderTargets::MakeGL` (query `GL_FRAMEBUFFER_BINDING` — GTK renders to its
  own FBO, not 0).
- Keep the CPU path as fallback (a `--software` flag); `SkiaMeterRenderer` already
  takes a plain `SkCanvas*` so it is backend-agnostic. Only the `IMeterWidget`
  implementation should change.
- Risk: GTK4 GL area uses GLES or GL depending on the session; test both
  (`GDK_DEBUG=gl-prefer-gl`).

### 7. Resolution-independent layout polish
`SkiaMeterRenderer::draw` scales the logical 1640×560 uniformly... but non-16:9-ish
window shapes stretch segments. Options: letterbox (preserve aspect, pad with screen
bg) or recompute layout from actual W/H (the constants are already `constexpr` ratios
— make them fractions of H instead of absolutes). Letterboxing is closer to the HTML
reference.

### 8. Configurable bands / FFT size
16 bands × 28 segments and FFT 4096 are compile-time constants in
`FftwSpectrumAnalyzer` / `SkiaMeterRenderer`. Making bands runtime (8/16/32/64)
requires:
- `std::array` → `std::vector` for levels/peaks/edges/labels.
- FFT size can stay 4096 (resolution is sufficient down to 30 Hz: bin ≈ 11.7 Hz @48k;
  the lowest band spans ~2 bins — already marginal, see #9).

### 9. Better low-frequency resolution
Bands 1–3 (37/54/80 Hz) cover only 2–4 FFT bins each, so they move in steps. Options,
in increasing effort: (a) interpolate magnitudes between bins; (b) zero-pad to 8192 for
display-only interpolation; (c) a parallel low-rate FFT (decimate ×8, second 4096 FFT
covering < 3 kHz with 1.46 Hz bins). Option (c) is what serious analyzers do; (a) is an
evening of work and visually adequate.

### 10. dB calibration & scale modes
The current mapping copies WebAudio's byte mapping (−100…−30 dBFS → 0…1) with a UI gain
multiplier on top. Add: a dBFS scale drawn on the left, selectable ranges
(−60/−90 dB), and optional A-weighting (apply per-bin weight table before band
averaging — table computed once in `buildBandEdges`).

---

## v0.4 — Packaging & project hygiene

### 11. Git + Skia provisioning
- `git init`; `.gitignore`: `build*/`, `third_party/skia/` (280 MB unpacked).
- Add `scripts/fetch-skia.sh` that downloads + unpacks the pinned release
  (`m148-a29c8d23be` from aseprite/skia) so a fresh clone is two commands. Optionally a
  CMake `ExternalProject`/`FetchContent` step doing the same — script is simpler.

### 12. Desktop integration
`dev.arme.SakiaVU.desktop`, an SVG icon (the meter motif), `install()` rules in CMake.
App ID is already `dev.arme.SakiaVU` in `src/app/AppController.cpp` — keep them in
sync.

### 13. Flatpak
PipeWire + GTK4 apps sandbox cleanly; needs `--socket=pulseaudio` replaced by the
PipeWire portal permission. The vendored static Skia keeps the manifest simple (no Skia
module needed — bundle the prebuilt or build it in the manifest from the same tag).

### 14. CI
GitHub Actions: Arch container job running `fetch-skia.sh`, build both targets, run
`render-test`, and (optionally) compare the PNG against a golden image with a small
pixel tolerance — `render_test.cpp` is deterministic (fixed seed, fixed frame count), so
golden-image testing is viable. Font availability in the container is the only
flakiness risk: install `ttf-dejavu` and the candidate list in
`src/ui/SkiaMeterRenderer.cpp:makeMonoTypeface` resolves identically.

---

## Known constraints (do not regress)

- **Never use `SkFontMgr_New_FontConfig`** from the prebuilt Skia — it hangs on
  aliasing-heavy fontconfig setups (this dev machine included). Typeface loading must
  stay file-path based (`SkFontMgr_New_Custom_Empty()->makeFromFile`). If font
  candidates need to grow, extend the path list, don't switch to fontconfig matching.
- The analyzer's ballistics (release 0.22, peak gravity 0.0009) are **per-frame at
  ~60 fps**, copied from the HTML's rAF loop. If the tick rate ever changes (GLArea
  vsync at 144 Hz!), convert them to per-second rates first or the meter feel changes:
  `release = 1 − pow(1 − 0.22, dt·60)`, `gravity = 0.0009 · (dt·60)²`-ish.
- `GtkMeterWidget` relies on `kBGRA_8888 + premul == CAIRO_FORMAT_ARGB32`
  (little-endian).
  Fine for x86_64/ARM64 Linux; revisit only if that assumption ever breaks.
- Prebuilt Skia is libstdc++ — if it's ever swapped for a libc++ build, the whole app
  must move to clang + libc++.
