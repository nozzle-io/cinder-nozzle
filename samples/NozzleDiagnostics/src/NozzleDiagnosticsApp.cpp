#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/Log.h"
#include "cinder/nozzle/CinderNozzle.h"

class NozzleDiagnosticsApp : public ci::app::App {
public:
    void setup() override {
        CI_LOG_I(cinder::nozzle::capture_diagnostics().summary());
        quit();
    }
};

CINDER_APP(NozzleDiagnosticsApp, ci::app::RendererGl)
