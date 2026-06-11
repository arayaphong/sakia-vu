#pragma once

#include <gtk/gtk.h>

#include "include/core/SkSurface.h"

#include "MeterRenderer.h"
#include "SpectrumAnalyzer.h"

// GtkDrawingArea that renders the meter through Skia (CPU raster) and blits
// the BGRA pixels to cairo. Rendering happens at device-pixel resolution.
class MeterWidget {
public:
    MeterWidget(const SpectrumAnalyzer& analyzer, const bool& peakHold);

    GtkWidget* widget() const { return area_; }

private:
    static void drawFunc(GtkDrawingArea* area, cairo_t* cr, int width, int height,
                         gpointer user_data);

    void render(cairo_t* cr, int width, int height);

    GtkWidget* area_ = nullptr;
    const SpectrumAnalyzer& analyzer_;
    const bool& peakHold_;
    MeterRenderer renderer_;
    sk_sp<SkSurface> surface_;
};
