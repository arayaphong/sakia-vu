#include "AppController.h"

#include <stdexcept>

AppController::AppController(std::unique_ptr<IAudioSource> audioSource,
                             std::unique_ptr<ISpectrumAnalyzer> spectrumAnalyzer,
                             std::unique_ptr<IMeterWidgetFactory> meterWidgetFactory)
    : audioSource_(std::move(audioSource)),
      spectrumAnalyzer_(std::move(spectrumAnalyzer)),
      meterWidgetFactory_(std::move(meterWidgetFactory)) {
    if (!audioSource_ || !spectrumAnalyzer_ || !meterWidgetFactory_) {
        throw std::invalid_argument("AppController dependencies must not be null");
    }
    frame_.resize(spectrumAnalyzer_->sampleCount());
}

AppController::~AppController() {
    if (interfaceSettings) {
        g_object_unref(interfaceSettings);
    }
}

int AppController::run(int argc, char** argv) {
    GtkApplication* gtkApp = gtk_application_new("dev.arme.SakiaVU", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtkApp, "activate", G_CALLBACK(onActivateStatic), this);
    int status = g_application_run(G_APPLICATION(gtkApp), argc, argv);
    g_object_unref(gtkApp);
    return status;
}

gboolean AppController::onTickStatic(GtkWidget* widget, GdkFrameClock* clock, gpointer user_data) {
    return static_cast<AppController*>(user_data)->onTick(widget, clock);
}

void AppController::onToggleStatic(GtkButton* btn, gpointer user_data) {
    static_cast<AppController*>(user_data)->onToggle(btn);
}

void AppController::onPeakToggleStatic(GtkToggleButton* btn, gpointer user_data) {
    static_cast<AppController*>(user_data)->onPeakToggle(btn);
}

void AppController::onActivateStatic(GtkApplication* gtkApp, gpointer user_data) {
    static_cast<AppController*>(user_data)->onActivate(gtkApp);
}

void AppController::onColorSchemeChangedStatic(GSettings*, gchar*, gpointer user_data) {
    static_cast<AppController*>(user_data)->syncThemePreference();
}

void AppController::setStatusMarkup(const char* markup) {
    gtk_label_set_markup(GTK_LABEL(statusLabel), markup);
}

gboolean AppController::onTick(GtkWidget* widget, GdkFrameClock*) {
    if (audioSource_->running()) {
        spectrumAnalyzer_->setSampleRate(audioSource_->sampleRate());
        audioSource_->latest(frame_.data(), frame_.size());
        float gain = static_cast<float>(gtk_range_get_value(GTK_RANGE(gainScale)));
        spectrumAnalyzer_->update(frame_.data(), gain, peakHold);
        
        MeterState state = spectrumAnalyzer_->getState();
        state.peakHoldEnabled = peakHold;
        meter_->updateState(state);
        
        gtk_widget_queue_draw(widget);
    }
    return G_SOURCE_CONTINUE;
}

void AppController::onToggle(GtkButton* btn) {
    if (audioSource_->running()) {
        audioSource_->stop();
        spectrumAnalyzer_->reset();
        gtk_button_set_label(btn, "Start Mic");
        setStatusMarkup("<span foreground=\"#d64545\">● STOPPED</span>");
        
        MeterState state = spectrumAnalyzer_->getState();
        state.peakHoldEnabled = peakHold;
        meter_->updateState(state);
        gtk_widget_queue_draw(meter_->widget());
    } else if (audioSource_->start()) {
        gtk_button_set_label(btn, "Stop");
        setStatusMarkup("<span foreground=\"#2fb344\">● LIVE</span>");
    } else {
        setStatusMarkup("<span foreground=\"#d97706\">▲ MIC ERROR</span>");
    }
}

void AppController::onPeakToggle(GtkToggleButton* btn) {
    peakHold = gtk_toggle_button_get_active(btn);
    if (!peakHold) spectrumAnalyzer_->resetPeaks();
}

void AppController::initThemePreferenceSync() {
    if (interfaceSettings) {
        syncThemePreference();
        return;
    }

    GSettingsSchemaSource* source = g_settings_schema_source_get_default();
    if (!source) {
        return;
    }

    GSettingsSchema* schema =
        g_settings_schema_source_lookup(source, "org.gnome.desktop.interface", TRUE);
    if (!schema) {
        return;
    }

    bool hasColorScheme = g_settings_schema_has_key(schema, "color-scheme");
    g_settings_schema_unref(schema);
    if (!hasColorScheme) {
        return;
    }

    interfaceSettings = g_settings_new("org.gnome.desktop.interface");
    syncThemePreference();
    g_signal_connect(interfaceSettings, "changed::color-scheme",
                     G_CALLBACK(onColorSchemeChangedStatic), this);
}

void AppController::syncThemePreference() {
    if (!interfaceSettings) {
        return;
    }

    gchar* colorScheme = g_settings_get_string(interfaceSettings, "color-scheme");
    gboolean prefersDark = g_strcmp0(colorScheme, "prefer-dark") == 0;
    g_free(colorScheme);

    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme",
                 prefersDark, nullptr);
}

void AppController::onActivate(GtkApplication* gtkApp) {
    initThemePreferenceSync();

    GtkWidget* window = gtk_application_window_new(gtkApp);
    gtk_window_set_title(GTK_WINDOW(window), "SakiaVU");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 460);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 14);
    gtk_widget_set_margin_start(vbox, 18);
    gtk_widget_set_margin_end(vbox, 18);

    // Header: title + status.
    GtkWidget* head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget* title = gtk_label_new("SPECTRUM - 16-BAND METER");
    gtk_widget_set_hexpand(title, TRUE);
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    statusLabel = gtk_label_new(nullptr);
    setStatusMarkup("<span foreground=\"#d64545\">● STOPPED</span>");
    gtk_box_append(GTK_BOX(head), title);
    gtk_box_append(GTK_BOX(head), statusLabel);
    gtk_box_append(GTK_BOX(vbox), head);

    // Meter screen.
    meter_ = meterWidgetFactory_->create();
    GtkWidget* screen = gtk_frame_new(nullptr);
    gtk_frame_set_child(GTK_FRAME(screen), meter_->widget());
    gtk_box_append(GTK_BOX(vbox), screen);

    // Controls: start/stop, gain, peak hold.
    GtkWidget* controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);

    toggleBtn = gtk_button_new_with_label("Start Mic");
    g_signal_connect(toggleBtn, "clicked", G_CALLBACK(onToggleStatic), this);
    gtk_box_append(GTK_BOX(controls), toggleBtn);

    GtkWidget* gainLabel = gtk_label_new("GAIN");
    gtk_box_append(GTK_BOX(controls), gainLabel);

    gainScale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.5, 6.0, 0.1);
    gtk_range_set_value(GTK_RANGE(gainScale), 1.8);
    gtk_widget_set_hexpand(gainScale, TRUE);
    gtk_box_append(GTK_BOX(controls), gainScale);

    peakBtn = gtk_toggle_button_new_with_label("Peak Hold");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(peakBtn), TRUE);
    g_signal_connect(peakBtn, "toggled", G_CALLBACK(onPeakToggleStatic), this);
    gtk_box_append(GTK_BOX(controls), peakBtn);

    gtk_box_append(GTK_BOX(vbox), controls);

    gtk_window_set_child(GTK_WINDOW(window), vbox);
    gtk_widget_add_tick_callback(meter_->widget(), onTickStatic, this, nullptr);
    gtk_window_present(GTK_WINDOW(window));
}
