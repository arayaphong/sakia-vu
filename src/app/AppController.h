#pragma once

#include <gtk/gtk.h>
#include <memory>
#include <vector>

#include "core/interfaces/IAudioSource.h"
#include "core/interfaces/IMeterWidget.h"
#include "core/interfaces/IMeterWidgetFactory.h"
#include "core/interfaces/IPhysicsWorld.h"
#include "core/interfaces/ISpectrumAnalyzer.h"

class AppController {
public:
    AppController(std::unique_ptr<IAudioSource> audioSource,
                  std::unique_ptr<ISpectrumAnalyzer> spectrumAnalyzer,
                  std::unique_ptr<IMeterWidgetFactory> meterWidgetFactory,
                  std::unique_ptr<IPhysicsWorld> physicsWorld);
    ~AppController();

    int run(int argc, char** argv);

private:
    static gboolean onTickStatic(GtkWidget* widget, GdkFrameClock* clock, gpointer user_data);
    static void onToggleStatic(GtkButton* btn, gpointer user_data);
    static void onPeakToggleStatic(GtkToggleButton* btn, gpointer user_data);
    static void onActivateStatic(GtkApplication* gtkApp, gpointer user_data);
    static void onColorSchemeChangedStatic(GSettings* settings, gchar* key, gpointer user_data);
    static void onDeviceSelectedStatic(GObject* object, GParamSpec* pspec, gpointer user_data);
    static void onMicModeSelectedStatic(GtkButton* btn, gpointer user_data);
    static void onOutputModeSelectedStatic(GtkButton* btn, gpointer user_data);
    static void onGainChangedStatic(GtkRange* range, gpointer user_data);
    static void onPhysicsToggleStatic(GtkToggleButton* btn, gpointer user_data);
    static void onLowGravityToggleStatic(GtkToggleButton* btn, gpointer user_data);
    static void onDropBallStatic(GtkButton* btn, gpointer user_data);
    static void onDropBoxStatic(GtkButton* btn, gpointer user_data);
    static void onClearObjectsStatic(GtkButton* btn, gpointer user_data);

    gboolean onTick(GtkWidget* widget, GdkFrameClock* clock);
    void onToggle(GtkButton* btn);
    void onPeakToggle(GtkToggleButton* btn);
    void onPhysicsToggle(GtkToggleButton* btn);
    void onActivate(GtkApplication* gtkApp);
    void onDeviceSelected();
    void onGainChanged();
    void setCaptureMode(AudioCaptureMode mode);
    bool restartCapture();
    void refreshDeviceList();
    void initThemePreferenceSync();
    void syncThemePreference();
    void setStatusMarkup(const char* markup);
    void updateListenButton();
    void updateModeMenuChecks();

    std::unique_ptr<IAudioSource> audioSource_;
    std::unique_ptr<ISpectrumAnalyzer> spectrumAnalyzer_;
    std::unique_ptr<IMeterWidgetFactory> meterWidgetFactory_;
    std::unique_ptr<IPhysicsWorld> physics_;
    std::unique_ptr<IMeterWidget> meter_;

    GtkWidget* toggleBtn = nullptr;
    GtkWidget* peakBtn = nullptr;
    GtkWidget* gainScale = nullptr;
    GtkWidget* gainValueLabel = nullptr;
    GtkWidget* statusLabel = nullptr;
    GtkWidget* listenIcon = nullptr;
    GtkWidget* listenLabel = nullptr;
    GtkWidget* captureModeMenuBtn = nullptr;
    GtkWidget* captureModePopover = nullptr;
    GtkWidget* micModeCheck = nullptr;
    GtkWidget* outputModeCheck = nullptr;
    GtkWidget* deviceDropDown = nullptr;
    GtkWidget* physicsBtn = nullptr;
    GtkWidget* physicsControls = nullptr;
    GSettings* interfaceSettings = nullptr;

    bool peakHold = true;
    bool physicsEnabled_ = false;
    bool updatingDeviceList = false;
    gint64 lastTickUs_ = 0;
    MeterState lastState_;
    std::vector<AudioDevice> audioDevices_;
    std::vector<float> frame_;
};
