#include "AppController.h"
#include "audio/FftwSpectrumAnalyzer.h"
#include "audio/PipeWireAudioCapture.h"
#include "physics/Box2dPhysicsWorld.h"
#include "ui/GtkMeterWidget.h"

#include <memory>

int main(int argc, char** argv) {
    AppController app(std::make_unique<PipeWireAudioCapture>(),
                      std::make_unique<FftwSpectrumAnalyzer>(),
                      std::make_unique<GtkMeterWidgetFactory>(),
                      std::make_unique<Box2dPhysicsWorld>());
    return app.run(argc, argv);
}
