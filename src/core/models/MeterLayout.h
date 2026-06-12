#pragma once

#include "MeterState.h"

// Logical-canvas layout shared by the Skia renderer and the physics world so
// collision geometry can never diverge from the drawn pixels. All values are
// in logical canvas pixels (1640x560).
namespace meterlayout {

constexpr float kLogicalW = 1640.0f;
constexpr float kLogicalH = 560.0f;
constexpr float kPadX = 24.0f;
constexpr float kPadTop = 14.0f;
constexpr float kPadBottom = 46.0f;
constexpr int kSegments = 28;
constexpr float kGapSeg = 4.0f;

constexpr float kUsableW = kLogicalW - kPadX * 2;                                // 1592
constexpr float kUsableH = kLogicalH - kPadTop - kPadBottom;                     // 500
constexpr float kSegH = (kUsableH - (kSegments - 1) * kGapSeg) / kSegments;      // 14
constexpr float kColW = kUsableW / MeterState::kNumBands;                        // 99.5
constexpr float kBarW = kColW * 0.62f;
constexpr float kGroundY = kLogicalH - kPadBottom;                               // 514

constexpr float barCenterX(int band) {
    return kPadX + band * kColW + kColW / 2;
}

// Number of lit segments for a level; matches the renderer's lround(level * 28).
constexpr int litSegments(float level) {
    return static_cast<int>(level * kSegments + 0.5f);
}

// Top edge (logical px) of the highest lit segment; ground line when unlit.
constexpr float barTopForLevel(float level) {
    int lit = litSegments(level);
    if (lit <= 0) return kGroundY;
    if (lit > kSegments) lit = kSegments;
    return kPadTop + (kSegments - lit) * (kSegH + kGapSeg);
}

} // namespace meterlayout
