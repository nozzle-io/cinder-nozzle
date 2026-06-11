#include "cinder/nozzle/NozzleSender.h"

#include <utility>

namespace cinder::nozzle {

sender::sender(std::string name) : name_(std::move(name)) {}
sender::~sender() { stop(); }
const std::string &sender::name() const { return name_; }
void sender::stop() { stopped_ = true; }

status sender::publish_texture(const ci::gl::TextureRef &texture, texture_format) {
    if (stopped_) {
        return {path_status::fail, "sender is stopped"};
    }
    if (!texture) {
        return {path_status::fail, "texture is null"};
    }
#if CINDER_NOZZLE_HAS_CINDER_GL
    return {path_status::missing_host_smoke, "Cinder GL headers are available, but runtime context/texture smoke is not recorded"};
#else
    return {path_status::missing_host_smoke, "Cinder GL headers are unavailable in this host-independent build"};
#endif
}

} // namespace cinder::nozzle
