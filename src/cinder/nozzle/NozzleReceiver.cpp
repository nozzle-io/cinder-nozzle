#include "cinder/nozzle/NozzleReceiver.h"

#include <utility>

namespace cinder::nozzle {

receiver::receiver(std::string source_name) : source_name_(std::move(source_name)) {}
receiver::~receiver() { stop(); }
const std::string &receiver::source_name() const { return source_name_; }
void receiver::stop() { stopped_ = true; }

status receiver::try_update_texture(const ci::gl::TextureRef &texture) {
    if (stopped_) {
        return {path_status::fail, "receiver is stopped"};
    }
    if (!texture) {
        return {path_status::fail, "texture is null"};
    }
    return {path_status::missing_host_smoke, "receiver GL texture update needs a real Cinder RendererGl smoke"};
}

} // namespace cinder::nozzle
