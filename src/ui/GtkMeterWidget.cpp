#include "GtkMeterWidget.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"

GtkMeterWidget::GtkMeterWidget() {
    area_ = gtk_drawing_area_new();
    gtk_widget_set_hexpand(area_, TRUE);
    gtk_widget_set_vexpand(area_, TRUE);
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(area_), 820);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(area_), 280);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area_), drawFunc, this, nullptr);

    GtkGesture* click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0); // any button
    g_signal_connect(click, "pressed", G_CALLBACK(onPressed), this);
    gtk_widget_add_controller(area_, GTK_EVENT_CONTROLLER(click));
}

void GtkMeterWidget::onPressed(GtkGestureClick* gesture, int, double x, double y,
                               gpointer user_data) {
    auto* self = static_cast<GtkMeterWidget*>(user_data);
    if (!self->spawnCb_) return;

    int w = gtk_widget_get_width(self->area_);
    int h = gtk_widget_get_height(self->area_);
    if (w <= 0 || h <= 0) return;

    // The renderer stretches the logical canvas to the full widget, so the
    // widget->logical mapping is a plain ratio.
    float lx = static_cast<float>(x) * SkiaMeterRenderer::kLogicalW / w;
    float ly = static_cast<float>(y) * SkiaMeterRenderer::kLogicalH / h;
    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
    self->spawnCb_(lx, ly, button == GDK_BUTTON_SECONDARY);
}

void GtkMeterWidget::drawFunc(GtkDrawingArea* area, cairo_t* cr, int width, int height,
                              gpointer user_data) {
    static_cast<GtkMeterWidget*>(user_data)->render(cr, width, height);
}

void GtkMeterWidget::render(cairo_t* cr, int width, int height) {
    int scale = gtk_widget_get_scale_factor(area_);
    int pw = width * scale, ph = height * scale;

    if (!surface_ || surface_->width() != pw || surface_->height() != ph) {
        surface_ = SkSurfaces::Raster(
            SkImageInfo::Make(pw, ph, kBGRA_8888_SkColorType, kPremul_SkAlphaType));
        if (!surface_) return;
    }

    renderer_.draw(surface_->getCanvas(), pw, ph, state_);
    renderer_.drawPhysicsOverlay(surface_->getCanvas(), pw, ph, physicsState_);

    SkPixmap pixmap;
    if (!surface_->peekPixels(&pixmap)) return;

    cairo_surface_t* cs = cairo_image_surface_create_for_data(
        static_cast<unsigned char*>(pixmap.writable_addr()), CAIRO_FORMAT_ARGB32,
        pw, ph, static_cast<int>(pixmap.rowBytes()));
    cairo_save(cr);
    cairo_scale(cr, 1.0 / scale, 1.0 / scale);
    cairo_set_source_surface(cr, cs, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
    cairo_surface_destroy(cs);
}

void GtkMeterWidget::updateState(const MeterState& state) {
    state_ = state;
}

void GtkMeterWidget::updatePhysicsState(const PhysicsState& state) {
    physicsState_ = state;
}

void GtkMeterWidget::setSpawnCallback(
    std::function<void(float lx, float ly, bool secondary)> cb) {
    spawnCb_ = std::move(cb);
}

std::unique_ptr<IMeterWidget> GtkMeterWidgetFactory::create() const {
    return std::make_unique<GtkMeterWidget>();
}
