// Offscreen smoke test: feeds a synthetic multi-tone signal through the
// analyzer and renders the meter to a PNG, so the visual output can be
// checked without a compositor.
#include <cmath>
#include <cstdio>

#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"
#include "include/encode/SkPngEncoder.h"

#include "MeterRenderer.h"
#include "SpectrumAnalyzer.h"

int main(int argc, char** argv) {
    const char* outPath = argc > 1 ? argv[1] : "/tmp/sakia-render.png";

    SpectrumAnalyzer analyzer;
    analyzer.setSampleRate(48000);

    // Mix of tones spread across the bands plus pink-ish noise.
    constexpr int N = SpectrumAnalyzer::kFftSize;
    float samples[N];
    const float tones[] = {60, 150, 400, 1000, 2500, 6000, 12000};
    unsigned seed = 1;
    for (int i = 0; i < N; i++) {
        float t = static_cast<float>(i) / 48000.0f;
        float v = 0.0f;
        for (float f : tones) v += 0.12f * std::sin(2.0f * M_PI * f * t);
        seed = seed * 1664525u + 1013904223u;
        v += 0.02f * (static_cast<float>(seed >> 16) / 32768.0f - 1.0f);
        samples[i] = v;
    }

    // Several frames so smoothing/peaks settle like a live run.
    for (int frame = 0; frame < 30; frame++)
        analyzer.update(samples, 1.8f, true);

    sk_sp<SkSurface> surface = SkSurfaces::Raster(
        SkImageInfo::Make(1640, 560, kN32_SkColorType, kPremul_SkAlphaType));
    MeterRenderer renderer;
    renderer.draw(surface->getCanvas(), 1640, 560, analyzer, true);

    SkPixmap pixmap;
    if (!surface->peekPixels(&pixmap)) {
        std::fprintf(stderr, "peekPixels failed\n");
        return 1;
    }
    SkFILEWStream out(outPath);
    if (!SkPngEncoder::Encode(&out, pixmap, {})) {
        std::fprintf(stderr, "PNG encode failed\n");
        return 1;
    }
    std::printf("wrote %s\n", outPath);
    return 0;
}
