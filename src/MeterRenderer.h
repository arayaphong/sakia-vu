#pragma once

#include <array>
#include <string>

#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkRefCnt.h"

#include "SpectrumAnalyzer.h"

// Draws the 16-band LED meter onto a Skia canvas.
// Logical canvas size is 1640x560, scaled uniformly to the actual widget size.
class MeterRenderer {
public:
    static constexpr int kSegments = 28;
    static constexpr float kLogicalW = 1640.0f;
    static constexpr float kLogicalH = 560.0f;

    MeterRenderer();

    void draw(SkCanvas* canvas, int width, int height,
              const SpectrumAnalyzer& analyzer, bool peakHold) const;

private:
    SkFont labelFont_;
};
