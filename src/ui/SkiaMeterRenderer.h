#pragma once

#include <array>
#include <string>

#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkRefCnt.h"

#include "core/models/MeterLayout.h"
#include "core/models/MeterState.h"
#include "core/models/PhysicsState.h"

// Draws the 16-band LED meter onto a Skia canvas.
// Logical canvas size is 1640x560, scaled uniformly to the actual widget size.
class SkiaMeterRenderer {
public:
    static constexpr int kSegments = meterlayout::kSegments;
    static constexpr float kLogicalW = meterlayout::kLogicalW;
    static constexpr float kLogicalH = meterlayout::kLogicalH;

    SkiaMeterRenderer();

    void draw(SkCanvas* canvas, int width, int height,
              const MeterState& state) const;

    // Physics playground objects, drawn on top of the meter with the same
    // logical-canvas scaling so they always align with the bars.
    void drawPhysicsOverlay(SkCanvas* canvas, int width, int height,
                            const PhysicsState& state) const;

private:
    SkFont labelFont_;
};
