#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/nozzle/CinderNozzle.h"

class NozzleSenderBasicApp : public ci::app::App {
public:
    void setup() override { sender_ = std::make_unique<cinder::nozzle::sender>("cinder-sender"); }
    void draw() override {
        auto texture = ci::gl::Texture2d::create(320, 240);
        auto status = sender_->publish_texture(texture);
        CI_LOG_I(status.message);
    }
    void cleanup() override { sender_->stop(); }
private:
    std::unique_ptr<cinder::nozzle::sender> sender_;
};

CINDER_APP(NozzleSenderBasicApp, ci::app::RendererGl)
