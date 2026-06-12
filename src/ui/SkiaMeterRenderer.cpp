#include "SkiaMeterRenderer.h"

#include "include/core/SkBlurTypes.h"
#include "include/core/SkColor.h"
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

SkiaMeterRenderer::SkiaMeterRenderer() {
    labelFont_ = SkFont(makeMonoTypeface(), 22.0f);
    labelFont_.setEdging(SkFont::Edging::kAntiAlias);
}

void SkiaMeterRenderer::draw(SkCanvas* canvas, int width, int height,
                             const MeterState& state) const {
    canvas->clear(0xFF070A0D); // .screen background

    // Scale the logical 1640x560 layout to fill the canvas.
    canvas->save();
    canvas->scale(width / kLogicalW, height / kLogicalH);

    // Layout values are shared with the physics world via MeterLayout.h.
    constexpr float W = kLogicalW, H = kLogicalH;
    constexpr float padX = meterlayout::kPadX, padTop = meterlayout::kPadTop;
    constexpr int bands = MeterState::kNumBands;
    constexpr float colW = meterlayout::kColW;
    constexpr float barW = meterlayout::kBarW;
    constexpr float gapSeg = meterlayout::kGapSeg;
    constexpr float segH = meterlayout::kSegH;

    const auto& levels = state.levels;
    const auto& peaks = state.peaks;
    const auto& labels = state.labels;

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
            bool isPeak = state.peakHoldEnabled && s == peakSeg - 1 && peakSeg > 0;

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

void SkiaMeterRenderer::drawPhysicsOverlay(SkCanvas* canvas, int width, int height,
                                           const PhysicsState& state) const {
    if (state.objects.empty()) return;

    canvas->save();
    canvas->scale(width / kLogicalW, height / kLogicalH);

    SkPaint fill;
    fill.setAntiAlias(true);
    SkPaint specular;
    specular.setAntiAlias(true);
    specular.setColor(SK_ColorWHITE);
    specular.setAlphaf(0.25f);

    for (const PhysicsObject& o : state.objects) {
        SkScalar hsv[3] = {o.hue, 0.75f, 0.95f}; // ~ the demo's hsl(h,80%,60%)
        fill.setColor(SkHSVToColor(hsv));

        canvas->save();
        canvas->translate(o.x, o.y);
        canvas->rotate(o.angle * 180.0f / SK_FloatPI);

        if (o.kind == PhysicsObject::Kind::Ball) {
            canvas->drawCircle(0, 0, o.size, fill);
            canvas->drawCircle(-o.size * 0.3f, -o.size * 0.3f, o.size * 0.35f,
                               specular);
        } else {
            SkRRect rrect = SkRRect::MakeRectXY(
                SkRect::MakeLTRB(-o.size, -o.size, o.size, o.size), 6, 6);
            canvas->drawRRect(rrect, fill);
        }
        canvas->restore();
    }

    canvas->restore();
}
