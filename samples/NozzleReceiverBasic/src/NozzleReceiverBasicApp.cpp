#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/nozzle/CinderNozzle.h"

class NozzleReceiverBasicApp : public ci::app::App {
public:
    void setup() override { receiver_ = std::make_unique<cinder::nozzle::receiver>("cinder-sender"); }
    void draw() override {
        auto texture = ci::gl::Texture2d::create(641, 479);
        auto status = receiver_->try_update_texture(texture);
        CI_LOG_I(status.message);
    }
    void cleanup() override { receiver_->stop(); }
private:
    std::unique_ptr<cinder::nozzle::receiver> receiver_;
};

CINDER_APP(NozzleReceiverBasicApp, ci::app::RendererGl)
