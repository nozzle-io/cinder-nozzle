#include "cinder/nozzle/NozzleSender.h"

#include "nozzle/nozzle_c.h"

#include <sstream>
#include <utility>

namespace cinder::nozzle {
namespace {

NozzleTextureFormat to_nozzle_format(texture_format format) {
    switch(format) {
        case texture_format::rgba8_unorm: return NOZZLE_FORMAT_RGBA8_UNORM;
        case texture_format::bgra8_unorm: return NOZZLE_FORMAT_BGRA8_UNORM;
        default: return NOZZLE_FORMAT_UNKNOWN;
    }
}

status error_status(const char *operation, NozzleErrorCode code) {
    std::ostringstream message;
    message << operation << " failed: " << static_cast<int>(code);
    return {path_status::fail, message.str()};
}

} // namespace

sender::sender(std::string name) : name_(std::move(name)) {}
sender::~sender() { stop(); }
const std::string &sender::name() const { return name_; }

void sender::stop() {
    if(sender_ != nullptr) {
        nozzle_sender_destroy(sender_);
        sender_ = nullptr;
    }
    stopped_ = true;
}

status sender::publish_texture(const ci::gl::TextureRef &texture, texture_format format) {
    if(stopped_) {
        return {path_status::fail, "sender is stopped"};
    }
    if(!texture) {
        return {path_status::fail, "texture is null"};
    }
#if CINDER_NOZZLE_HAS_CINDER_GL
    if(texture->getId() == 0 || texture->getWidth() <= 0 || texture->getHeight() <= 0) {
        return {path_status::fail, "sender texture is invalid"};
    }
    if(texture->isTopDown()) {
        return {path_status::fail, "top-down sender textures are unsupported until origin-aware GL publish is available"};
    }
    const auto nozzle_format = to_nozzle_format(format);
    if(nozzle_format == NOZZLE_FORMAT_UNKNOWN) {
        return {path_status::fail, "unsupported texture format"};
    }
    if(sender_ == nullptr) {
        NozzleSenderDesc desc{};
        desc.name = name_.c_str();
        desc.application_name = "cinder-nozzle";
        desc.ring_buffer_size = 3;
        desc.allow_format_fallback = 1;
        auto code = nozzle_sender_create(&desc, &sender_);
        if(code != NOZZLE_OK) {
            sender_ = nullptr;
            return error_status("nozzle_sender_create", code);
        }
    }
    auto code = nozzle_sender_publish_gl_texture(
        sender_,
        static_cast<std::uint32_t>(texture->getId()),
        static_cast<std::uint32_t>(texture->getTarget()),
        static_cast<std::uint32_t>(texture->getWidth()),
        static_cast<std::uint32_t>(texture->getHeight()),
        nozzle_format);
    if(code != NOZZLE_OK) {
        return error_status("nozzle_sender_publish_gl_texture", code);
    }
    return {path_status::pass, "Cinder TextureRef published through nozzle GL interop"};
#else
    return {path_status::missing_host_smoke, "Cinder GL headers are unavailable in this host-independent build"};
#endif
}

} // namespace cinder::nozzle
