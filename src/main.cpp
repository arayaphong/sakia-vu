#include <gtk/gtk.h>

#include <array>
#include <memory>

#include "AudioCapture.h"
#include "MeterWidget.h"
#include "SpectrumAnalyzer.h"

namespace {

struct App {
    AudioCapture capture;
    SpectrumAnalyzer analyzer;
    std::unique_ptr<MeterWidget> meter;

    GtkWidget* toggleBtn = nullptr;
    GtkWidget* peakBtn = nullptr;
    GtkWidget* gainScale = nullptr;
    GtkWidget* statusLabel = nullptr;

    bool peakHold = true;
    std::array<float, SpectrumAnalyzer::kFftSize> frame{};
};

gboolean onTick(GtkWidget* widget, GdkFrameClock*, gpointer user_data) {
    auto* app = static_cast<App*>(user_data);
    if (app->capture.running()) {
        app->analyzer.setSampleRate(app->capture.sampleRate());
        app->capture.latest(app->frame.data(), app->frame.size());
        float gain = static_cast<float>(gtk_range_get_value(GTK_RANGE(app->gainScale)));
        app->analyzer.update(app->frame.data(), gain, app->peakHold);
        gtk_widget_queue_draw(widget);
    }
    return G_SOURCE_CONTINUE;
}

void onToggle(GtkButton* btn, gpointer user_data) {
    auto* app = static_cast<App*>(user_data);
    if (app->capture.running()) {
        app->capture.stop();
        app->analyzer.reset();
        gtk_button_set_label(btn, "Start Mic");
        gtk_widget_remove_css_class(GTK_WIDGET(btn), "on");
        gtk_label_set_text(GTK_LABEL(app->statusLabel), "STOPPED");
        gtk_widget_remove_css_class(app->statusLabel, "live");
        gtk_widget_queue_draw(app->meter->widget());
    } else if (app->capture.start()) {
        gtk_button_set_label(btn, "Stop");
        gtk_widget_add_css_class(GTK_WIDGET(btn), "on");
        gtk_label_set_text(GTK_LABEL(app->statusLabel), "LIVE");
        gtk_widget_add_css_class(app->statusLabel, "live");
    } else {
        gtk_label_set_text(GTK_LABEL(app->statusLabel), "MIC ERROR");
    }
}

void onPeakToggle(GtkToggleButton* btn, gpointer user_data) {
    auto* app = static_cast<App*>(user_data);
    app->peakHold = gtk_toggle_button_get_active(btn);
    if (!app->peakHold) app->analyzer.resetPeaks();
}

void loadCss() {
    static const char* css = R"(
        window { background: #0b0d10; }
        .console { background: #10141a; }
        .screen-frame {
            background: #070a0d;
            border: 1px solid #000000;
            border-radius: 12px;
            padding: 10px;
        }
        .title-label {
            font-family: monospace;
            font-size: 13px;
            letter-spacing: 3px;
            color: #6b7682;
        }
        .status-label {
            font-family: monospace;
            font-size: 11px;
            letter-spacing: 2px;
            color: #6b7682;
        }
        .status-label.live { color: #ff4d52; }
        button {
            font-family: monospace;
            font-size: 12px;
            color: #e8edf2;
            background: #1c2228;
            border: 1px solid #272d35;
            border-radius: 9px;
        }
        button.on, button:checked {
            color: #08110b;
            background: #2bc466;
            border-color: #2bc466;
        }
        scale trough { background: #2a3138; }
        label { color: #6b7682; font-family: monospace; font-size: 11px; }
    )";
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, css);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

void onActivate(GtkApplication* gtkApp, gpointer user_data) {
    auto* app = static_cast<App*>(user_data);

    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme",
                 TRUE, nullptr);
    loadCss();

    GtkWidget* window = gtk_application_window_new(gtkApp);
    gtk_window_set_title(GTK_WINDOW(window), "SakiaVU");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 460);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(vbox, "console");
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 14);
    gtk_widget_set_margin_start(vbox, 18);
    gtk_widget_set_margin_end(vbox, 18);

    // Header: title + status.
    GtkWidget* head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget* title = gtk_label_new("SPECTRUM \xc2\xb7 16-BAND METER");
    gtk_widget_add_css_class(title, "title-label");
    gtk_widget_set_hexpand(title, TRUE);
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    app->statusLabel = gtk_label_new("STOPPED");
    gtk_widget_add_css_class(app->statusLabel, "status-label");
    gtk_box_append(GTK_BOX(head), title);
    gtk_box_append(GTK_BOX(head), app->statusLabel);
    gtk_box_append(GTK_BOX(vbox), head);

    // Meter screen.
    app->meter = std::make_unique<MeterWidget>(app->analyzer, app->peakHold);
    GtkWidget* screen = gtk_frame_new(nullptr);
    gtk_widget_add_css_class(screen, "screen-frame");
    gtk_frame_set_child(GTK_FRAME(screen), app->meter->widget());
    gtk_box_append(GTK_BOX(vbox), screen);

    // Controls: start/stop, gain, peak hold.
    GtkWidget* controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);

    app->toggleBtn = gtk_button_new_with_label("Start Mic");
    g_signal_connect(app->toggleBtn, "clicked", G_CALLBACK(onToggle), app);
    gtk_box_append(GTK_BOX(controls), app->toggleBtn);

    GtkWidget* gainLabel = gtk_label_new("GAIN");
    gtk_box_append(GTK_BOX(controls), gainLabel);

    app->gainScale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.5, 6.0, 0.1);
    gtk_range_set_value(GTK_RANGE(app->gainScale), 1.8);
    gtk_widget_set_hexpand(app->gainScale, TRUE);
    gtk_box_append(GTK_BOX(controls), app->gainScale);

    app->peakBtn = gtk_toggle_button_new_with_label("Peak Hold");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->peakBtn), TRUE);
    g_signal_connect(app->peakBtn, "toggled", G_CALLBACK(onPeakToggle), app);
    gtk_box_append(GTK_BOX(controls), app->peakBtn);

    gtk_box_append(GTK_BOX(vbox), controls);

    gtk_window_set_child(GTK_WINDOW(window), vbox);
    gtk_widget_add_tick_callback(app->meter->widget(), onTick, app, nullptr);
    gtk_window_present(GTK_WINDOW(window));
}

} // namespace

int main(int argc, char** argv) {
    App app;
    GtkApplication* gtkApp =
        gtk_application_new("dev.arme.SakiaVU", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtkApp, "activate", G_CALLBACK(onActivate), &app);
    int status = g_application_run(G_APPLICATION(gtkApp), argc, argv);
    g_object_unref(gtkApp);
    return status;
}
