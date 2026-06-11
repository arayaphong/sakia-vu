# SakiaVU

**SakiaVU** is a real-time 16-band spectrum/VU meter desktop application for Linux, written in C++ using GTK4 and Skia for rendering. It captures audio via PipeWire and performs frequency analysis using FFTW3.

## Tech Stack
- **Language**: C++20
- **Build System**: CMake (>= 3.25) + Ninja
- **UI Framework**: GTK 4
- **Rendering**: Skia (vendored aseprite m148 prebuilt)
- **Audio Capture**: PipeWire (`libpipewire-0.3`)
- **DSP**: FFTW3 (`fftw3f` - single-precision float)

## Building and Running

### Prerequisites
Ensure the system has the required dependencies (`gtk4`, `pipewire`, `pipewire-alsa`, `fftw`, `cmake`, `ninja`).

**Prebuilt Skia:**
Skia is vendored as a prebuilt static library and must be downloaded to `third_party/skia` before building:
```bash
mkdir -p third_party && cd third_party
curl -L -o skia.zip https://github.com/aseprite/skia/releases/download/m148-a29c8d23be/Skia-Linux-Release-x64.zip
mkdir skia && cd skia && bsdtar xf ../skia.zip && rm ../skia.zip
```

### Build Commands
```bash
# Configure the project
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build build

# Run the application
./build/sakia-vu

# Run offscreen render smoke test (generates a PNG image)
cmake --build build --target render-test
./build/render-test /tmp/sakia-render.png
```

## Architecture Flow
`IAudioSource` (implemented by `PipeWireAudioCapture`) -> `ISpectrumAnalyzer` (implemented by `FftwSpectrumAnalyzer`) -> `MeterState` -> `IMeterWidget` (implemented by `GtkMeterWidget`) -> `SkiaMeterRenderer` (Skia drawing at 1640x560, cairo blit through GTK).

For the full source-tree map, dependency direction, and layer rules, see
`docs/ARCHITECTURE.md`. The composition root is `src/app/main.cpp`; `AppController`
should receive abstractions through constructor injection.

## Development Conventions & Known Constraints

**CRITICAL RULES:**
- **Font Loading:** NEVER use `SkFontMgr_New_FontConfig` from the prebuilt Skia - it causes severe hangs due to aliasing-heavy fontconfig setups. Always load fonts directly from file paths (using `SkFontMgr_New_Custom_Empty()->makeFromFile()`).
- **Animation Ballistics:** Analyzer ballistics (release 0.22, peak gravity 0.0009) are currently calculated **per-frame** assuming a ~60 fps tick rate (matching an HTML rAF loop). If the tick rate becomes variable or targets higher refresh rates (e.g., 144Hz via `GtkGLArea`), these MUST be converted to time-based per-second rates.
- **Pixel Formats:** The `GtkMeterWidget` relies on the assumption that Skia's `kBGRA_8888 + premul` format perfectly aligns with `CAIRO_FORMAT_ARGB32` on little-endian architectures (x86_64/ARM64 Linux).
- **Toolchain:** The vendored prebuilt Skia is compiled against `libstdc++`. The project must be built with standard GCC/libstdc++ (or clang + libstdc++) to link correctly.
- **Signal Processing:** The lowest frequency bands only cover a few FFT bins and thus move in larger steps. There is a roadmap item to improve low-frequency resolution.

See `docs/ROADMAP.md` for upcoming tasks, including GPU rendering (GtkGLArea + Skia Ganesh), stereo support, input device selection, and packaging plans.
