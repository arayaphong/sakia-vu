#pragma once

#include <gtk/gtk.h>
#include <memory>
#include <vector>

#include "core/interfaces/IAudioSource.h"
#include "core/interfaces/IMeterWidget.h"
#include "core/interfaces/IMeterWidgetFactory.h"
#include "core/interfaces/ISpectrumAnalyzer.h"

class AppController {
public:
    AppController(std::unique_ptr<IAudioSource> audioSource,
                  std::unique_ptr<ISpectrumAnalyzer> spectrumAnalyzer,
                  std::unique_ptr<IMeterWidgetFactory> meterWidgetFactory);
    ~AppController();

    int run(int argc, char** argv);

private:
    static gboolean onTickStatic(GtkWidget* widget, GdkFrameClock* clock, gpointer user_data);
    static void onToggleStatic(GtkButton* btn, gpointer user_data);
    static void onPeakToggleStatic(GtkToggleButton* btn, gpointer user_data);
    static void onActivateStatic(GtkApplication* gtkApp, gpointer user_data);
    static void onColorSchemeChangedStatic(GSettings* settings, gchar* key, gpointer user_data);
    static void onDeviceSelectedStatic(GObject* object, GParamSpec* pspec, gpointer user_data);
    static void onCaptureModeToggledStatic(GtkToggleButton* btn, gpointer user_data);

    gboolean onTick(GtkWidget* widget, GdkFrameClock* clock);
    void onToggle(GtkButton* btn);
    void onPeakToggle(GtkToggleButton* btn);
    void onActivate(GtkApplication* gtkApp);
    void onDeviceSelected();
    void onCaptureModeToggled(GtkToggleButton* btn);
    bool restartCapture();
    void refreshDeviceList();
    void initThemePreferenceSync();
    void syncThemePreference();
    void setStatusMarkup(const char* markup);

    std::unique_ptr<IAudioSource> audioSource_;
    std::unique_ptr<ISpectrumAnalyzer> spectrumAnalyzer_;
    std::unique_ptr<IMeterWidgetFactory> meterWidgetFactory_;
    std::unique_ptr<IMeterWidget> meter_;

    GtkWidget* toggleBtn = nullptr;
    GtkWidget* peakBtn = nullptr;
    GtkWidget* gainScale = nullptr;
    GtkWidget* statusLabel = nullptr;
    GtkWidget* captureModeBtn = nullptr;
    GtkWidget* deviceDropDown = nullptr;
    GSettings* interfaceSettings = nullptr;

    bool peakHold = true;
    bool updatingDeviceList = false;
    std::vector<AudioDevice> audioDevices_;
    std::vector<float> frame_;
};
