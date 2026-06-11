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

std::unique_ptr<IMeterWidget> GtkMeterWidgetFactory::create() const {
    return std::make_unique<GtkMeterWidget>();
}
