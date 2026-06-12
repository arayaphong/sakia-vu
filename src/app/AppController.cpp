#include "AppController.h"

#include <stdexcept>

AppController::AppController(std::unique_ptr<IAudioSource> audioSource,
                             std::unique_ptr<ISpectrumAnalyzer> spectrumAnalyzer,
                             std::unique_ptr<IMeterWidgetFactory> meterWidgetFactory,
                             std::unique_ptr<IPhysicsWorld> physicsWorld)
    : audioSource_(std::move(audioSource)),
      spectrumAnalyzer_(std::move(spectrumAnalyzer)),
      meterWidgetFactory_(std::move(meterWidgetFactory)),
      physics_(std::move(physicsWorld)) {
    if (!audioSource_ || !spectrumAnalyzer_ || !meterWidgetFactory_ || !physics_) {
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

void AppController::onDeviceSelectedStatic(GObject*, GParamSpec*, gpointer user_data) {
    static_cast<AppController*>(user_data)->onDeviceSelected();
}

void AppController::onMicModeSelectedStatic(GtkButton*, gpointer user_data) {
    static_cast<AppController*>(user_data)->setCaptureMode(AudioCaptureMode::Microphone);
}

void AppController::onOutputModeSelectedStatic(GtkButton*, gpointer user_data) {
    static_cast<AppController*>(user_data)->setCaptureMode(AudioCaptureMode::Output);
}

void AppController::onGainChangedStatic(GtkRange*, gpointer user_data) {
    static_cast<AppController*>(user_data)->onGainChanged();
}

void AppController::onPhysicsToggleStatic(GtkToggleButton* btn, gpointer user_data) {
    static_cast<AppController*>(user_data)->onPhysicsToggle(btn);
}

void AppController::onLowGravityToggleStatic(GtkToggleButton* btn, gpointer user_data) {
    auto* self = static_cast<AppController*>(user_data);
    self->physics_->setLowGravity(gtk_toggle_button_get_active(btn));
}

void AppController::onDropBallStatic(GtkButton*, gpointer user_data) {
    // Negative coords: random x, spawn above the bars.
    static_cast<AppController*>(user_data)->physics_->spawnBall(-1.0f, -1.0f);
}

void AppController::onDropBoxStatic(GtkButton*, gpointer user_data) {
    static_cast<AppController*>(user_data)->physics_->spawnBox(-1.0f, -1.0f);
}

void AppController::onClearObjectsStatic(GtkButton*, gpointer user_data) {
    static_cast<AppController*>(user_data)->physics_->clear();
}

void AppController::setStatusMarkup(const char* markup) {
    gtk_label_set_markup(GTK_LABEL(statusLabel), markup);
}

void AppController::updateListenButton() {
    const char* iconName = audioSource_->captureMode() == AudioCaptureMode::Output
                               ? "audio-speakers-symbolic"
                               : "audio-input-microphone-symbolic";
    gtk_image_set_from_icon_name(GTK_IMAGE(listenIcon), iconName);
    gtk_label_set_text(GTK_LABEL(listenLabel), audioSource_->running() ? "Stop" : "Listen");
}

void AppController::updateModeMenuChecks() {
    bool outputMode = audioSource_->captureMode() == AudioCaptureMode::Output;
    gtk_widget_set_opacity(micModeCheck, outputMode ? 0.0 : 1.0);
    gtk_widget_set_opacity(outputModeCheck, outputMode ? 1.0 : 0.0);
}

gboolean AppController::onTick(GtkWidget* widget, GdkFrameClock* clock) {
    bool running = audioSource_->running();
    if (running) {
        spectrumAnalyzer_->setSampleRate(audioSource_->sampleRate());
        audioSource_->latest(frame_.data(), frame_.size());
        float gain = static_cast<float>(gtk_range_get_value(GTK_RANGE(gainScale)));
        spectrumAnalyzer_->update(frame_.data(), gain, peakHold);

        MeterState state = spectrumAnalyzer_->getState();
        state.peakHoldEnabled = peakHold;
        lastState_ = state;
        meter_->updateState(state);
    }

    if (physicsEnabled_) {
        // Physics uses real frame time (sub-stepped at a fixed rate inside),
        // unlike the analyzer ballistics which assume ~60 fps.
        gint64 now = gdk_frame_clock_get_frame_time(clock);
        float dt = lastTickUs_ > 0 ? static_cast<float>(now - lastTickUs_) * 1e-6f
                                   : 1.0f / 60.0f;
        lastTickUs_ = now;

        lastState_.peakHoldEnabled = peakHold; // may change while stopped
        physics_->step(dt, lastState_);
        meter_->updatePhysicsState(physics_->state());
    } else {
        lastTickUs_ = 0;
    }

    if (running || physicsEnabled_) {
        gtk_widget_queue_draw(widget);
    }
    return G_SOURCE_CONTINUE;
}

void AppController::onToggle(GtkButton* btn) {
    if (audioSource_->running()) {
        audioSource_->stop();
        spectrumAnalyzer_->reset();
        updateListenButton();
        setStatusMarkup("<span foreground=\"#d64545\">● STOPPED</span>");
        
        MeterState state = spectrumAnalyzer_->getState();
        state.peakHoldEnabled = peakHold;
        meter_->updateState(state);
        gtk_widget_queue_draw(meter_->widget());
    } else if (audioSource_->start()) {
        updateListenButton();
        setStatusMarkup("<span foreground=\"#2fb344\">● LIVE</span>");
    } else {
        setStatusMarkup("<span foreground=\"#d97706\">▲ CAPTURE ERROR</span>");
    }
}

void AppController::onPeakToggle(GtkToggleButton* btn) {
    peakHold = gtk_toggle_button_get_active(btn);
    if (!peakHold) spectrumAnalyzer_->resetPeaks();
}

void AppController::onPhysicsToggle(GtkToggleButton* btn) {
    physicsEnabled_ = gtk_toggle_button_get_active(btn);
    gtk_widget_set_visible(physicsControls, physicsEnabled_);
    if (!physicsEnabled_) {
        // Keep the world so objects resume where they were on re-enable,
        // but clear the overlay immediately.
        meter_->updatePhysicsState(PhysicsState{});
        gtk_widget_queue_draw(meter_->widget());
    }
}

void AppController::onDeviceSelected() {
    if (updatingDeviceList || !deviceDropDown) {
        return;
    }

    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(deviceDropDown));
    if (selected == GTK_INVALID_LIST_POSITION || selected >= audioDevices_.size()) {
        return;
    }

    audioSource_->setDeviceTarget(audioDevices_[selected].targetObject);
    if (!audioSource_->running()) {
        return;
    }

    restartCapture();
}

void AppController::onGainChanged() {
    gchar* text = g_strdup_printf("%.1fx", gtk_range_get_value(GTK_RANGE(gainScale)));
    gtk_label_set_text(GTK_LABEL(gainValueLabel), text);
    g_free(text);
}

void AppController::setCaptureMode(AudioCaptureMode mode) {
    if (audioSource_->captureMode() == mode) {
        gtk_popover_popdown(GTK_POPOVER(captureModePopover));
        return;
    }

    audioSource_->setCaptureMode(mode);
    audioSource_->setDeviceTarget("");
    refreshDeviceList();
    updateListenButton();
    updateModeMenuChecks();
    gtk_popover_popdown(GTK_POPOVER(captureModePopover));

    if (audioSource_->running()) {
        restartCapture();
    }
}

bool AppController::restartCapture() {
    audioSource_->stop();
    spectrumAnalyzer_->reset();
    if (audioSource_->start()) {
        updateListenButton();
        setStatusMarkup("<span foreground=\"#2fb344\">● LIVE</span>");
        return true;
    } else {
        updateListenButton();
        setStatusMarkup("<span foreground=\"#d97706\">▲ CAPTURE ERROR</span>");
        return false;
    }
}

void AppController::refreshDeviceList() {
    audioDevices_.clear();
    bool outputMode = audioSource_->captureMode() == AudioCaptureMode::Output;
    audioDevices_.push_back({"", outputMode ? "Default output" : "Default input"});

    std::vector<AudioDevice> devices = audioSource_->devices();
    audioDevices_.insert(audioDevices_.end(), devices.begin(), devices.end());

    GtkStringList* model = gtk_string_list_new(nullptr);
    for (const AudioDevice& device : audioDevices_) {
        gtk_string_list_append(model, device.displayName.c_str());
    }

    updatingDeviceList = true;
    gtk_drop_down_set_model(GTK_DROP_DOWN(deviceDropDown), G_LIST_MODEL(model));
    g_object_unref(model);

    guint selected = 0;
    const std::string& target = audioSource_->deviceTarget();
    for (guint i = 0; i < audioDevices_.size(); ++i) {
        if (audioDevices_[i].targetObject == target) {
            selected = i;
            break;
        }
    }
    gtk_drop_down_set_selected(GTK_DROP_DOWN(deviceDropDown), selected);
    updatingDeviceList = false;
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

    // Click-to-spawn physics objects: left = ball, right = box.
    meter_->setSpawnCallback([this](float lx, float ly, bool secondary) {
        if (!physicsEnabled_) return;
        if (secondary)
            physics_->spawnBox(lx, ly);
        else
            physics_->spawnBall(lx, ly);
    });

    // Controls: start/stop, gain, peak hold.
    GtkWidget* controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);

    GtkWidget* captureGroup = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(captureGroup, "linked");

    toggleBtn = gtk_button_new();
    GtkWidget* listenContent = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    listenIcon = gtk_image_new();
    listenLabel = gtk_label_new(nullptr);
    gtk_box_append(GTK_BOX(listenContent), listenIcon);
    gtk_box_append(GTK_BOX(listenContent), listenLabel);
    gtk_button_set_child(GTK_BUTTON(toggleBtn), listenContent);
    updateListenButton();
    g_signal_connect(toggleBtn, "clicked", G_CALLBACK(onToggleStatic), this);
    gtk_box_append(GTK_BOX(captureGroup), toggleBtn);

    captureModeMenuBtn = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(captureModeMenuBtn), "pan-down-symbolic");
    gtk_widget_set_tooltip_text(captureModeMenuBtn, "Choose capture mode");

    captureModePopover = gtk_popover_new();
    GtkWidget* modeBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* micModeBtn = gtk_button_new();
    GtkWidget* micModeRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    micModeCheck = gtk_image_new_from_icon_name("object-select-symbolic");
    gtk_box_append(GTK_BOX(micModeRow), micModeCheck);
    gtk_box_append(GTK_BOX(micModeRow), gtk_label_new("Mic"));
    gtk_button_set_child(GTK_BUTTON(micModeBtn), micModeRow);

    GtkWidget* outputModeBtn = gtk_button_new();
    GtkWidget* outputModeRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    outputModeCheck = gtk_image_new_from_icon_name("object-select-symbolic");
    gtk_box_append(GTK_BOX(outputModeRow), outputModeCheck);
    gtk_box_append(GTK_BOX(outputModeRow), gtk_label_new("Output"));
    gtk_button_set_child(GTK_BUTTON(outputModeBtn), outputModeRow);
    updateModeMenuChecks();

    g_signal_connect(micModeBtn, "clicked", G_CALLBACK(onMicModeSelectedStatic), this);
    g_signal_connect(outputModeBtn, "clicked", G_CALLBACK(onOutputModeSelectedStatic), this);
    gtk_box_append(GTK_BOX(modeBox), micModeBtn);
    gtk_box_append(GTK_BOX(modeBox), outputModeBtn);
    gtk_popover_set_child(GTK_POPOVER(captureModePopover), modeBox);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(captureModeMenuBtn), captureModePopover);
    gtk_box_append(GTK_BOX(captureGroup), captureModeMenuBtn);
    gtk_box_append(GTK_BOX(controls), captureGroup);

    deviceDropDown = gtk_drop_down_new(nullptr, nullptr);
    gtk_widget_set_hexpand(deviceDropDown, TRUE);
    g_signal_connect(deviceDropDown, "notify::selected", G_CALLBACK(onDeviceSelectedStatic), this);
    gtk_box_append(GTK_BOX(controls), deviceDropDown);
    refreshDeviceList();

    GtkWidget* gainLabel = gtk_label_new("GAIN");
    gtk_box_append(GTK_BOX(controls), gainLabel);

    gainScale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.1, 1.5, 0.1);
    gtk_range_set_value(GTK_RANGE(gainScale), 1.0);
    gtk_widget_set_size_request(gainScale, 180, -1);
    g_signal_connect(gainScale, "value-changed", G_CALLBACK(onGainChangedStatic), this);
    gtk_box_append(GTK_BOX(controls), gainScale);

    gainValueLabel = gtk_label_new(nullptr);
    gtk_widget_set_size_request(gainValueLabel, 42, -1);
    onGainChanged();
    gtk_box_append(GTK_BOX(controls), gainValueLabel);

    peakBtn = gtk_toggle_button_new_with_label("Peak Hold");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(peakBtn), TRUE);
    g_signal_connect(peakBtn, "toggled", G_CALLBACK(onPeakToggleStatic), this);
    gtk_box_append(GTK_BOX(controls), peakBtn);

    physicsBtn = gtk_toggle_button_new_with_label("Physics");
    g_signal_connect(physicsBtn, "toggled", G_CALLBACK(onPhysicsToggleStatic), this);
    gtk_box_append(GTK_BOX(controls), physicsBtn);

    gtk_box_append(GTK_BOX(vbox), controls);

    // Physics playground controls, hidden until the Physics toggle is on.
    physicsControls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);

    GtkWidget* dropBallBtn = gtk_button_new_with_label("Drop Ball");
    g_signal_connect(dropBallBtn, "clicked", G_CALLBACK(onDropBallStatic), this);
    gtk_box_append(GTK_BOX(physicsControls), dropBallBtn);

    GtkWidget* dropBoxBtn = gtk_button_new_with_label("Drop Box");
    g_signal_connect(dropBoxBtn, "clicked", G_CALLBACK(onDropBoxStatic), this);
    gtk_box_append(GTK_BOX(physicsControls), dropBoxBtn);

    GtkWidget* clearBtn = gtk_button_new_with_label("Clear");
    g_signal_connect(clearBtn, "clicked", G_CALLBACK(onClearObjectsStatic), this);
    gtk_box_append(GTK_BOX(physicsControls), clearBtn);

    GtkWidget* lowGravBtn = gtk_toggle_button_new_with_label("Low Gravity");
    g_signal_connect(lowGravBtn, "toggled", G_CALLBACK(onLowGravityToggleStatic), this);
    gtk_box_append(GTK_BOX(physicsControls), lowGravBtn);

    GtkWidget* spawnHint = gtk_label_new("Click canvas: ball · right-click: box");
    gtk_widget_set_opacity(spawnHint, 0.6);
    gtk_box_append(GTK_BOX(physicsControls), spawnHint);

    gtk_widget_set_visible(physicsControls, FALSE);
    gtk_box_append(GTK_BOX(vbox), physicsControls);

    gtk_window_set_child(GTK_WINDOW(window), vbox);
    gtk_widget_add_tick_callback(meter_->widget(), onTickStatic, this, nullptr);
    gtk_window_present(GTK_WINDOW(window));
}
