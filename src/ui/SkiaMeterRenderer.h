#pragma once

#include <array>
#include <string>

#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkRefCnt.h"

#include "core/models/MeterState.h"

// Draws the 16-band LED meter onto a Skia canvas.
// Logical canvas size is 1640x560, scaled uniformly to the actual widget size.
class SkiaMeterRenderer {
public:
    static constexpr int kSegments = 28;
    static constexpr float kLogicalW = 1640.0f;
    static constexpr float kLogicalH = 560.0f;

    SkiaMeterRenderer();

    void draw(SkCanvas* canvas, int width, int height,
              const MeterState& state) const;

private:
    SkFont labelFont_;
};
