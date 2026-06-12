#pragma once

#include <gtk/gtk.h>

#include "include/core/SkSurface.h"

#include "core/interfaces/IMeterWidget.h"
#include "core/interfaces/IMeterWidgetFactory.h"
#include "SkiaMeterRenderer.h"

// GtkDrawingArea that renders the meter through Skia (CPU raster) and blits
// the BGRA pixels to cairo. Rendering happens at device-pixel resolution.
class GtkMeterWidget final : public IMeterWidget {
public:
    GtkMeterWidget();
    void updateState(const MeterState& state) override;
    void updatePhysicsState(const PhysicsState& state) override;
    void setSpawnCallback(
        std::function<void(float lx, float ly, bool secondary)> cb) override;

    GtkWidget* widget() const override { return area_; }

private:
    static void drawFunc(GtkDrawingArea* area, cairo_t* cr, int width, int height,
                         gpointer user_data);
    static void onPressed(GtkGestureClick* gesture, int n_press, double x, double y,
                          gpointer user_data);

    void render(cairo_t* cr, int width, int height);

    GtkWidget* area_ = nullptr;
    MeterState state_;
    PhysicsState physicsState_;
    std::function<void(float, float, bool)> spawnCb_;
    SkiaMeterRenderer renderer_;
    sk_sp<SkSurface> surface_;
};

class GtkMeterWidgetFactory final : public IMeterWidgetFactory {
public:
    std::unique_ptr<IMeterWidget> create() const override;
};
