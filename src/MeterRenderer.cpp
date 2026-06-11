#include "MeterRenderer.h"

#include "include/core/SkBlurTypes.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkRRect.h"
#include "include/core/SkTypeface.h"
#include "include/ports/SkFontMgr_empty.h"

#include <cmath>
#include <filesystem>

namespace {
constexpr SkColor kGreen = 0xFF34E07A;
constexpr SkColor kAmber = 0xFFFFC24B;
constexpr SkColor kRed = 0xFFFF4D52;
constexpr SkColor kUnlit = 0xFF161B20;
constexpr SkColor kLabel = 0xFF6B7682;
constexpr SkColor kUnit = 0xFF3A424C;

SkColor segColor(float ratio) {
    if (ratio > 0.82f) return kRed;
    if (ratio > 0.6f) return kAmber;
    return kGreen;
}

sk_sp<SkTypeface> makeMonoTypeface() {
    // Fontconfig-based lookup is avoided on purpose: the prebuilt Skia's
    // SkFontMgr_New_FontConfig can spin for minutes enumerating family names
    // on hosts with aliasing-heavy fontconfig setups. Load a known TTF
    // directly through the FreeType-only manager instead.
    static sk_sp<SkFontMgr> mgr = SkFontMgr_New_Custom_Empty();
    const char* candidates[] = {
        "/usr/share/fonts/TTF/JetBrainsMono-SemiBold.ttf",
        "/usr/share/fonts/TTF/JetBrainsMono-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/liberation/LiberationMono-Bold.ttf",
        "/usr/share/fonts/noto/NotoSansMono-Bold.ttf",
    };
    for (const char* path : candidates) {
        if (!std::filesystem::exists(path)) continue;
        if (sk_sp<SkTypeface> tf = mgr->makeFromFile(path, 0)) return tf;
    }
    return nullptr; // SkFont copes with a null typeface (falls back to empty glyphs)
}
} // namespace

MeterRenderer::MeterRenderer() {
    labelFont_ = SkFont(makeMonoTypeface(), 22.0f);
    labelFont_.setEdging(SkFont::Edging::kAntiAlias);
}

void MeterRenderer::draw(SkCanvas* canvas, int width, int height,
                         const SpectrumAnalyzer& analyzer, bool peakHold) const {
    canvas->clear(0xFF070A0D); // .screen background

    // Scale the logical 1640x560 layout to fill the canvas.
    canvas->save();
    canvas->scale(width / kLogicalW, height / kLogicalH);

    constexpr float W = kLogicalW, H = kLogicalH;
    constexpr float padX = 24, padTop = 14, padBottom = 46;
    constexpr float usableW = W - padX * 2;
    constexpr float usableH = H - padTop - padBottom;
    constexpr int bands = SpectrumAnalyzer::kNumBands;
    constexpr float colW = usableW / bands;
    constexpr float barW = colW * 0.62f;
    constexpr float gapSeg = 4;
    constexpr float segH = (usableH - (kSegments - 1) * gapSeg) / kSegments;

    const auto& levels = analyzer.levels();
    const auto& peaks = analyzer.peaks();
    const auto& labels = analyzer.labels();

    SkPaint fill;
    fill.setAntiAlias(true);

    SkPaint glow;
    glow.setAntiAlias(true);

    for (int b = 0; b < bands; b++) {
        float x = padX + b * colW + (colW - barW) / 2;
        int lit = static_cast<int>(std::lround(levels[b] * kSegments));
        int peakSeg = static_cast<int>(std::lround(peaks[b] * kSegments));

        for (int s = 0; s < kSegments; s++) {
            float ratio = static_cast<float>(s + 1) / kSegments;
            float y = padTop + (kSegments - 1 - s) * (segH + gapSeg);
            bool isLit = s < lit;
            bool isPeak = peakHold && s == peakSeg - 1 && peakSeg > 0;

            SkRRect rrect = SkRRect::MakeRectXY(SkRect::MakeXYWH(x, y, barW, segH), 2, 2);

            if (isLit || isPeak) {
                SkColor c = segColor(ratio);
                // Emulate canvas shadowBlur with a blurred under-layer.
                float blur = isPeak ? 14.0f : 8.0f;
                glow.setColor(c);
                glow.setMaskFilter(
                    SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur * 0.5f));
                canvas->drawRRect(rrect, glow);

                fill.setColor(c);
                fill.setAlphaf(isPeak ? 1.0f : 0.95f);
            } else {
                fill.setColor(kUnlit);
                fill.setAlphaf(1.0f);
            }
            canvas->drawRRect(rrect, fill);
        }

        // Frequency label under the bar.
        SkPaint textPaint;
        textPaint.setAntiAlias(true);
        textPaint.setColor(kLabel);
        const std::string& label = labels[b];
        float tw = labelFont_.measureText(label.c_str(), label.size(),
                                          SkTextEncoding::kUTF8);
        canvas->drawSimpleText(label.c_str(), label.size(), SkTextEncoding::kUTF8,
                               x + barW / 2 - tw / 2, H - 16, labelFont_, textPaint);
    }

    // "Hz" unit, bottom right.
    SkPaint unitPaint;
    unitPaint.setAntiAlias(true);
    unitPaint.setColor(kUnit);
    SkFont unitFont = labelFont_;
    unitFont.setSize(20.0f);
    const char* hz = "Hz";
    float tw = unitFont.measureText(hz, 2, SkTextEncoding::kUTF8);
    canvas->drawSimpleText(hz, 2, SkTextEncoding::kUTF8, W - padX - tw, H - 16,
                           unitFont, unitPaint);

    canvas->restore();
}
