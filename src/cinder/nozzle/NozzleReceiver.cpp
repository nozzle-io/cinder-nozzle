#include "cinder/nozzle/NozzleReceiver.h"

#include "nozzle/nozzle_c.h"

#if CINDER_NOZZLE_HAS_CINDER_GL
#include "cinder/ip/Flip.h"
#endif

#include <cstdint>
#include <limits>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace cinder::nozzle {
namespace {

NozzleTextureFormat destination_format_from_frame(NozzleFrame *frame) {
    NozzleFrameInfo info{};
    if(nozzle_frame_get_info(frame, &info) != NOZZLE_OK) {
        return NOZZLE_FORMAT_UNKNOWN;
    }
    if(info.format == NOZZLE_FORMAT_RGBA8_UNORM || info.format == NOZZLE_FORMAT_BGRA8_UNORM) {
        return info.format;
    }
    if(info.semantic_format == NOZZLE_FORMAT_RGBA8_UNORM || info.semantic_format == NOZZLE_FORMAT_BGRA8_UNORM) {
        return info.semantic_format;
    }
    return NOZZLE_FORMAT_RGBA8_UNORM;
}

status error_status(const char *operation, NozzleErrorCode code) {
    std::ostringstream message;
    message << operation << " failed: " << static_cast<int>(code);
    return {path_status::fail, message.str()};
}


struct frame_deleter {
    void operator()(NozzleFrame *frame) const noexcept {
        if(frame != nullptr) {
            nozzle_frame_release(frame);
        }
    }
};

#if CINDER_NOZZLE_HAS_CINDER_GL
bool mapped_pixels_to_surface(const NozzleMappedPixels &mapped, ci::Surface8u *surface) {
    if(surface == nullptr || mapped.data == nullptr) {
        return false;
    }
    if(mapped.width == 0 || mapped.height == 0) {
        return false;
    }
    if(mapped.origin != NOZZLE_ORIGIN_TOP_LEFT) {
        return false;
    }
    if(mapped.format != NOZZLE_FORMAT_RGBA8_UNORM && mapped.format != NOZZLE_FORMAT_BGRA8_UNORM) {
        return false;
    }
    if(std::numeric_limits<int64_t>::max() / 4 < static_cast<int64_t>(mapped.width)) {
        return false;
    }
    const auto expected_stride = static_cast<int64_t>(mapped.width) * 4;
    if(mapped.row_stride_bytes != expected_stride) {
        return false;
    }

    *surface = ci::Surface8u(static_cast<int32_t>(mapped.width), static_cast<int32_t>(mapped.height), true, ci::SurfaceChannelOrder::RGBA);
    const auto *base = static_cast<const std::uint8_t *>(mapped.data);
    for(std::uint32_t y = 0; y < mapped.height; ++y) {
        const auto *source_row = base + static_cast<std::size_t>(y) * static_cast<std::size_t>(expected_stride);
        for(std::uint32_t x = 0; x < mapped.width; ++x) {
            const auto *source_pixel = source_row + static_cast<std::size_t>(x) * 4u;
            auto *destination_pixel = surface->getData(ci::ivec2(static_cast<int32_t>(x), static_cast<int32_t>(y)));
            if(mapped.format == NOZZLE_FORMAT_BGRA8_UNORM) {
                destination_pixel[surface->getRedOffset()] = source_pixel[2u];
                destination_pixel[surface->getGreenOffset()] = source_pixel[1u];
                destination_pixel[surface->getBlueOffset()] = source_pixel[0u];
                destination_pixel[surface->getAlphaOffset()] = source_pixel[3u];
            } else {
                destination_pixel[surface->getRedOffset()] = source_pixel[0u];
                destination_pixel[surface->getGreenOffset()] = source_pixel[1u];
                destination_pixel[surface->getBlueOffset()] = source_pixel[2u];
                destination_pixel[surface->getAlphaOffset()] = source_pixel[3u];
            }
        }
    }
    return true;
}

status copy_frame_to_texture_cpu(NozzleFrame *frame, const ci::gl::TextureRef &texture, NozzleErrorCode gl_code) {
    NozzleFrameInfo info{};
    auto code = nozzle_frame_get_info(frame, &info);
    if(code != NOZZLE_OK) {
        return error_status("nozzle_frame_get_info", code);
    }

    const auto texture_width = texture->getWidth();
    const auto texture_height = texture->getHeight();
    if(texture_width <= 0 || texture_height <= 0) {
        return {path_status::fail, "receiver texture dimensions are invalid"};
    }
    if(info.width != static_cast<std::uint32_t>(texture_width) ||
       info.height != static_cast<std::uint32_t>(texture_height)) {
        return {path_status::fail, "receiver texture dimensions do not match frame"};
    }
    if(static_cast<std::uint32_t>(std::numeric_limits<int32_t>::max()) < info.width ||
       static_cast<std::uint32_t>(std::numeric_limits<int32_t>::max()) < info.height) {
        return {path_status::fail, "frame dimensions exceed Cinder Surface8u limits"};
    }

    const auto pixel_count = static_cast<std::uint64_t>(info.width) * static_cast<std::uint64_t>(info.height);
    if(pixel_count == 0 || std::numeric_limits<std::uint64_t>::max() / 4u < pixel_count) {
        return {path_status::fail, "frame dimensions overflow"};
    }
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(pixel_count * 4u));
    NozzleMappedPixels mapped{};
    code = nozzle_frame_copy_pixels_with_origin(frame, NOZZLE_ORIGIN_TOP_LEFT, pixels.data(), pixels.size(), &mapped);
    if(code != NOZZLE_OK) {
        return error_status("nozzle_frame_copy_pixels_with_origin", code);
    }
    if(mapped.width != info.width || mapped.height != info.height || mapped.origin != NOZZLE_ORIGIN_TOP_LEFT) {
        return {path_status::fail, "copied pixel metadata does not match requested frame"};
    }
    ci::Surface8u surface;
    if(!mapped_pixels_to_surface(mapped, &surface)) {
        return {path_status::fail, "copied pixels could not be converted to Cinder Surface8u"};
    }
    if(texture->isTopDown()) {
        texture->update(surface);
    } else {
        ci::Surface8u flipped(surface.getWidth(), surface.getHeight(), true, ci::SurfaceChannelOrder::RGBA);
        ci::ip::flipVertical(surface, &flipped);
        texture->update(flipped);
    }
    std::ostringstream message;
    message << "copy_path=cpu-fallback copy_cost=cpu-copy macos_iosurface_blit=FAIL gl_error=" << static_cast<int>(gl_code);
    return {path_status::pass, message.str()};
}

#endif

} // namespace

receiver::receiver(std::string source_name) : source_name_(std::move(source_name)) {}
receiver::~receiver() { stop(); }
const std::string &receiver::source_name() const { return source_name_; }

void receiver::stop() {
    if(receiver_ != nullptr) {
        nozzle_receiver_destroy(receiver_);
        receiver_ = nullptr;
    }
    stopped_ = true;
}

status receiver::try_update_texture(const ci::gl::TextureRef &texture) {
    if(stopped_) {
        return {path_status::fail, "receiver is stopped"};
    }
    if(!texture) {
        return {path_status::fail, "texture is null"};
    }
#if CINDER_NOZZLE_HAS_CINDER_GL
    if(texture->getId() == 0 || texture->getWidth() <= 0 || texture->getHeight() <= 0) {
        return {path_status::fail, "receiver texture is invalid"};
    }
    if(receiver_ == nullptr) {
        NozzleReceiverDesc desc{};
        desc.name = source_name_.c_str();
        desc.application_name = "cinder-nozzle";
        desc.receive_mode = NOZZLE_RECEIVE_LATEST_ONLY;
        auto code = nozzle_receiver_create(&desc, &receiver_);
        if(code != NOZZLE_OK) {
            receiver_ = nullptr;
            return error_status("nozzle_receiver_create", code);
        }
    }

    NozzleAcquireDesc acquire_desc{};
    acquire_desc.timeout_ms = 1000;
    NozzleFrame *frame = nullptr;
    auto code = nozzle_receiver_acquire_frame(receiver_, &acquire_desc, &frame);
    if(code != NOZZLE_OK) {
        return error_status("nozzle_receiver_acquire_frame", code);
    }
    if(frame == nullptr) {
        return {path_status::fail, "receiver acquired null frame"};
    }
    std::unique_ptr<NozzleFrame, frame_deleter> frame_guard(frame);

    const auto format = destination_format_from_frame(frame_guard.get());
    if(format == NOZZLE_FORMAT_UNKNOWN) {
        return {path_status::fail, "frame format is unavailable or unsupported"};
    }
    if(texture->isTopDown()) {
        return copy_frame_to_texture_cpu(frame_guard.get(), texture, NOZZLE_ERROR_UNSUPPORTED_BACKEND);
    }
    code = nozzle_frame_copy_to_gl_texture(
        frame_guard.get(),
        static_cast<std::uint32_t>(texture->getId()),
        static_cast<std::uint32_t>(texture->getTarget()),
        static_cast<std::uint32_t>(texture->getWidth()),
        static_cast<std::uint32_t>(texture->getHeight()),
        format);
    if(code != NOZZLE_OK) {
        return copy_frame_to_texture_cpu(frame_guard.get(), texture, code);
    }
    return {path_status::pass, "copy_path=gl copy_cost=gpu-copy macos_iosurface_blit=PASS"};
#else
    return {path_status::missing_host_smoke, "Cinder GL headers are unavailable in this host-independent build"};
#endif
}

} // namespace cinder::nozzle
