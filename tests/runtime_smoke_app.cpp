#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/nozzle/CinderNozzle.h"
#include "nozzle/nozzle_c.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> y_flipped(std::vector<std::uint8_t> rgba, std::uint32_t width, std::uint32_t height) {
    const auto row_bytes = static_cast<std::size_t>(width) * 4u;
    for (std::uint32_t y = 0; y < height / 2u; ++y) {
        auto *top = rgba.data() + static_cast<std::size_t>(y) * row_bytes;
        auto *bottom = rgba.data() + static_cast<std::size_t>(height - 1u - y) * row_bytes;
        std::swap_ranges(top, top + row_bytes, bottom);
    }
    return rgba;
}

std::vector<std::uint8_t> rb_swapped(std::vector<std::uint8_t> rgba) {
    for (std::size_t i = 0; i + 3u < rgba.size(); i += 4u) {
        std::swap(rgba[i + 0u], rgba[i + 2u]);
    }
    return rgba;
}

std::vector<std::uint8_t> alpha_mutated(std::vector<std::uint8_t> rgba) {
    if (rgba.size() > 7u) {
        rgba[3u] = static_cast<std::uint8_t>(rgba[3u] ^ 0xffu);
    }
    return rgba;
}

bool expect_pass(const std::vector<std::uint8_t> &rgba, std::uint32_t width, std::uint32_t height, const char *label) {
    const auto result = cinder::nozzle::assert_rgba_pattern(rgba, width, height);
    if (!result.ok()) {
        std::cerr << "CINDER_NOZZLE_RUNTIME_ORACLE_FAIL label=" << label << " message=" << result.message << "\n";
        return false;
    }
    return true;
}

bool expect_fail(const std::vector<std::uint8_t> &rgba, std::uint32_t width, std::uint32_t height, const char *label) {
    const auto result = cinder::nozzle::assert_rgba_pattern(rgba, width, height);
    if (result.ok()) {
        std::cerr << "CINDER_NOZZLE_RUNTIME_ORACLE_FAIL label=" << label << " message=unexpected-pass\n";
        return false;
    }
    return true;
}

bool exercise_pattern_oracle(std::uint32_t width, std::uint32_t height) {
    const auto pattern = cinder::nozzle::make_rgba_pattern(width, height);
    auto truncated = pattern;
    if (!truncated.empty()) {
        truncated.pop_back();
    }
    const bool ok = expect_pass(pattern, width, height, "positive") &&
                    expect_fail(y_flipped(pattern, width, height), width, height, "y-flip") &&
                    expect_fail(rb_swapped(pattern), width, height, "r-b-swap") &&
                    expect_fail(alpha_mutated(pattern), width, height, "alpha") &&
                    expect_fail(truncated, width, height, "byte-size");
    std::cout << "CINDER_NOZZLE_CPU_ORACLE size=" << width << "x" << height
              << " no_y_flip=" << (ok ? "PASS" : "FAIL")
              << " no_r_b_swap=" << (ok ? "PASS" : "FAIL")
              << " alpha=" << (ok ? "PASS" : "FAIL")
              << " byte_size_mismatch=" << (ok ? "PASS" : "FAIL") << "\n";
    return ok;
}

const char *nozzle_error_name(NozzleErrorCode code) {
    switch (code) {
        case NOZZLE_OK: return "NOZZLE_OK";
        case NOZZLE_ERROR_UNKNOWN: return "NOZZLE_ERROR_UNKNOWN";
        case NOZZLE_ERROR_INVALID_ARGUMENT: return "NOZZLE_ERROR_INVALID_ARGUMENT";
        case NOZZLE_ERROR_UNSUPPORTED_BACKEND: return "NOZZLE_ERROR_UNSUPPORTED_BACKEND";
        case NOZZLE_ERROR_UNSUPPORTED_FORMAT: return "NOZZLE_ERROR_UNSUPPORTED_FORMAT";
        case NOZZLE_ERROR_DEVICE_MISMATCH: return "NOZZLE_ERROR_DEVICE_MISMATCH";
        case NOZZLE_ERROR_RESOURCE_CREATION_FAILED: return "NOZZLE_ERROR_RESOURCE_CREATION_FAILED";
        case NOZZLE_ERROR_SHARED_HANDLE_FAILED: return "NOZZLE_ERROR_SHARED_HANDLE_FAILED";
        case NOZZLE_ERROR_SENDER_NOT_FOUND: return "NOZZLE_ERROR_SENDER_NOT_FOUND";
        case NOZZLE_ERROR_SENDER_CLOSED: return "NOZZLE_ERROR_SENDER_CLOSED";
        case NOZZLE_ERROR_TIMEOUT: return "NOZZLE_ERROR_TIMEOUT";
        case NOZZLE_ERROR_BACKEND_ERROR: return "NOZZLE_ERROR_BACKEND_ERROR";
        case NOZZLE_ERROR_COMMAND_FAILED: return "NOZZLE_ERROR_COMMAND_FAILED";
    }
    return "NOZZLE_ERROR_UNRECOGNIZED";
}

struct frame_interop_result {
    bool pass = false;
    std::string sender_status = "FAIL";
    std::string receiver_status = "FAIL";
    std::string texture_transfer_status = "MISSING_HOST_SMOKE";
    std::string copy_cost = "UNPROVEN";
    std::string mapped_format = "UNPROVEN";
    std::string copied_format = "UNPROVEN";
    std::string short_buffer_status = "UNPROVEN";
    std::string message;
};

void destroy_sender(NozzleSender **sender) {
    if (sender != nullptr && *sender != nullptr) {
        nozzle_sender_destroy(*sender);
        *sender = nullptr;
    }
}

void destroy_receiver(NozzleReceiver **receiver) {
    if (receiver != nullptr && *receiver != nullptr) {
        nozzle_receiver_destroy(*receiver);
        *receiver = nullptr;
    }
}

void release_frame(NozzleFrame **frame) {
    if (frame != nullptr && *frame != nullptr) {
        nozzle_frame_release(*frame);
        *frame = nullptr;
    }
}

const char *format_name(NozzleTextureFormat format) {
    switch (format) {
        case NOZZLE_FORMAT_RGBA8_UNORM: return "RGBA8_UNORM";
        case NOZZLE_FORMAT_BGRA8_UNORM: return "BGRA8_UNORM";
        default: return "UNSUPPORTED_FORMAT";
    }
}

std::string error_message(const char *operation, NozzleErrorCode code) {
    std::ostringstream ss;
    ss << operation << ": " << nozzle_error_name(code)
       << "(" << static_cast<int>(code) << ")";
    return ss.str();
}

bool checked_abs_i64_to_u64(std::int64_t value, std::uint64_t *out_abs_value) {
    if (out_abs_value == nullptr || value == std::numeric_limits<std::int64_t>::min()) {
        return false;
    }
    if (value < 0) {
        *out_abs_value = static_cast<std::uint64_t>(-(value + 1)) + 1u;
    } else {
        *out_abs_value = static_cast<std::uint64_t>(value);
    }
    return true;
}

bool is_supported_8bit_rgba_storage(NozzleTextureFormat format) {
    return format == NOZZLE_FORMAT_RGBA8_UNORM ||
           format == NOZZLE_FORMAT_BGRA8_UNORM;
}

void write_pixel_for_storage(
    std::uint8_t *destination,
    const std::uint8_t *rgba,
    NozzleTextureFormat format
) {
    if (format == NOZZLE_FORMAT_BGRA8_UNORM) {
        destination[0] = rgba[2];
        destination[1] = rgba[1];
        destination[2] = rgba[0];
        destination[3] = rgba[3];
        return;
    }
    destination[0] = rgba[0];
    destination[1] = rgba[1];
    destination[2] = rgba[2];
    destination[3] = rgba[3];
}

std::vector<std::uint8_t> storage_to_rgba(
    std::vector<std::uint8_t> bytes,
    NozzleTextureFormat format
) {
    if (format != NOZZLE_FORMAT_BGRA8_UNORM) {
        return bytes;
    }
    for (std::size_t i = 0; i + 3u < bytes.size(); i += 4u) {
        std::swap(bytes[i + 0u], bytes[i + 2u]);
    }
    return bytes;
}

std::string validate_mapping_metadata(
    const NozzleMappedPixels &mapped,
    std::uint32_t width,
    std::uint32_t height
) {
    if (mapped.width != width) {
        return "mapped-width-mismatch";
    }
    if (mapped.height != height) {
        return "mapped-height-mismatch";
    }
    if (mapped.origin != NOZZLE_ORIGIN_TOP_LEFT) {
        return "mapped-origin-mismatch";
    }
    if (!is_supported_8bit_rgba_storage(mapped.format)) {
        return std::string("mapped-format-unsupported=") + format_name(mapped.format);
    }
    const auto expected_row_bytes = static_cast<std::uint64_t>(width) * 4u;
    std::uint64_t absolute_stride{0};
    if (!checked_abs_i64_to_u64(mapped.row_stride_bytes, &absolute_stride)) {
        return "mapped-row-stride-invalid";
    }
    if (absolute_stride < expected_row_bytes) {
        return "mapped-row-stride-too-small";
    }
    return "ok";
}

std::string validate_copied_metadata(
    const NozzleMappedPixels &copied,
    std::uint32_t width,
    std::uint32_t height
) {
    if (copied.width != width) {
        return "copied-width-mismatch";
    }
    if (copied.height != height) {
        return "copied-height-mismatch";
    }
    if (copied.origin != NOZZLE_ORIGIN_TOP_LEFT) {
        return "copied-origin-mismatch";
    }
    if (!is_supported_8bit_rgba_storage(copied.format)) {
        return std::string("copied-format-unsupported=") + format_name(copied.format);
    }
    const auto expected_row_bytes = static_cast<std::int64_t>(width) * 4;
    if (copied.row_stride_bytes != expected_row_bytes) {
        return "copied-row-stride-mismatch";
    }
    return "ok";
}

bool copy_rgba_to_mapping(
    const std::vector<std::uint8_t> &rgba,
    const NozzleMappedPixels &mapped
) {
    if (mapped.data == nullptr ||
        mapped.width == 0 ||
        mapped.height == 0 ||
        !is_supported_8bit_rgba_storage(mapped.format)) {
        return false;
    }
    const auto source_row_bytes = static_cast<std::size_t>(mapped.width) * 4u;
    std::uint64_t absolute_stride{0};
    if (!checked_abs_i64_to_u64(mapped.row_stride_bytes, &absolute_stride) ||
        absolute_stride < static_cast<std::uint64_t>(source_row_bytes)) {
        return false;
    }
    const auto expected_bytes = source_row_bytes * static_cast<std::size_t>(mapped.height);
    if (rgba.size() != expected_bytes) {
        return false;
    }
    auto *base = static_cast<std::uint8_t *>(mapped.data);
    for (std::uint32_t y = 0; y < mapped.height; ++y) {
        auto *dst = base +
            static_cast<std::ptrdiff_t>(mapped.row_stride_bytes) *
            static_cast<std::ptrdiff_t>(y);
        const auto *src = rgba.data() +
            source_row_bytes * static_cast<std::size_t>(y);
        for (std::uint32_t x = 0; x < mapped.width; ++x) {
            write_pixel_for_storage(
                dst + static_cast<std::size_t>(x) * 4u,
                src + static_cast<std::size_t>(x) * 4u,
                mapped.format);
        }
    }
    return true;
}

frame_interop_result exercise_nozzle_cpu_frame_interop(
    std::uint32_t width,
    std::uint32_t height
) {
    frame_interop_result result;
    const auto pattern = cinder::nozzle::make_rgba_pattern(width, height);
    if (pattern.empty()) {
        result.message = "pattern-generation-failed";
        return result;
    }

    const std::string source_name =
        "cinder-runtime-smoke-" + std::to_string(width) +
        "x" + std::to_string(height);
    NozzleSenderDesc sender_desc{};
    sender_desc.name = source_name.c_str();
    sender_desc.application_name = "cinder-nozzle-runtime-smoke";
    sender_desc.ring_buffer_size = 2;
    sender_desc.fallback_flags = NOZZLE_FALLBACK_STORAGE_COMPATIBLE;
    sender_desc.fallback_flags_valid = 1;

    NozzleSender *sender = nullptr;
    auto code = nozzle_sender_create(&sender_desc, &sender);
    if (code != NOZZLE_OK) {
        result.sender_status = "MISSING_HOST_SMOKE";
        result.message = error_message("nozzle_sender_create", code);
        return result;
    }

    NozzleReceiverDesc receiver_desc{};
    receiver_desc.name = source_name.c_str();
    receiver_desc.application_name = "cinder-nozzle-runtime-smoke";
    receiver_desc.receive_mode = NOZZLE_RECEIVE_LATEST_ONLY;

    NozzleReceiver *receiver = nullptr;
    code = nozzle_receiver_create(&receiver_desc, &receiver);
    if (code != NOZZLE_OK) {
        result.receiver_status = "MISSING_HOST_SMOKE";
        result.message = error_message("nozzle_receiver_create", code);
        destroy_sender(&sender);
        return result;
    }

    NozzleFrame *writable_frame = nullptr;
    code = nozzle_sender_acquire_writable_frame(
        sender, width, height, NOZZLE_FORMAT_RGBA8_UNORM, &writable_frame);
    if (code != NOZZLE_OK) {
        result.message = error_message("nozzle_sender_acquire_writable_frame", code);
        destroy_receiver(&receiver);
        destroy_sender(&sender);
        return result;
    }

    NozzlePixelMapping *mapping = nullptr;
    NozzleMappedPixels mapped{};
    code = nozzle_frame_lock_writable_pixels_mapping_with_origin(
        writable_frame, NOZZLE_ORIGIN_TOP_LEFT, &mapping, &mapped);
    if (code != NOZZLE_OK) {
        result.message = error_message(
            "nozzle_frame_lock_writable_pixels_mapping_with_origin", code);
        nozzle_sender_discard_frame(sender, writable_frame);
        release_frame(&writable_frame);
        destroy_receiver(&receiver);
        destroy_sender(&sender);
        return result;
    }

    result.mapped_format = format_name(mapped.format);
    const auto mapped_metadata_status = validate_mapping_metadata(mapped, width, height);
    if (mapped_metadata_status != "ok") {
        result.message = mapped_metadata_status;
        nozzle_pixel_mapping_unlock(&mapping);
        nozzle_sender_discard_frame(sender, writable_frame);
        release_frame(&writable_frame);
        destroy_receiver(&receiver);
        destroy_sender(&sender);
        return result;
    }

    if (!copy_rgba_to_mapping(pattern, mapped)) {
        result.message = std::string("copy_rgba_to_mapping-failed mapped_format=") +
            format_name(mapped.format);
        nozzle_pixel_mapping_unlock(&mapping);
        nozzle_sender_discard_frame(sender, writable_frame);
        release_frame(&writable_frame);
        destroy_receiver(&receiver);
        destroy_sender(&sender);
        return result;
    }

    code = nozzle_pixel_mapping_unlock_checked(&mapping);
    if (code != NOZZLE_OK) {
        result.message = error_message("nozzle_pixel_mapping_unlock_checked", code);
        nozzle_sender_discard_frame(sender, writable_frame);
        release_frame(&writable_frame);
        destroy_receiver(&receiver);
        destroy_sender(&sender);
        return result;
    }

    code = nozzle_sender_commit_frame(sender, writable_frame);
    if (code != NOZZLE_OK) {
        result.message = error_message("nozzle_sender_commit_frame", code);
        (void)nozzle_sender_discard_frame(sender, writable_frame);
        release_frame(&writable_frame);
        destroy_receiver(&receiver);
        destroy_sender(&sender);
        return result;
    }
    release_frame(&writable_frame);

    NozzleAcquireDesc acquire_desc{};
    acquire_desc.timeout_ms = 1000;
    NozzleFrame *received_frame = nullptr;
    code = nozzle_receiver_acquire_frame(receiver, &acquire_desc, &received_frame);
    if (code != NOZZLE_OK) {
        result.message = error_message("nozzle_receiver_acquire_frame", code);
        destroy_receiver(&receiver);
        destroy_sender(&sender);
        return result;
    }

    std::vector<std::uint8_t> short_buffer(pattern.size() - 1u);
    NozzleMappedPixels short_copy{};
    code = nozzle_frame_copy_pixels_with_origin(
        received_frame,
        NOZZLE_ORIGIN_TOP_LEFT,
        short_buffer.data(),
        short_buffer.size(),
        &short_copy);
    if (code != NOZZLE_ERROR_INVALID_ARGUMENT) {
        result.message = error_message("short-buffer-copy", code);
        release_frame(&received_frame);
        destroy_receiver(&receiver);
        destroy_sender(&sender);
        return result;
    }
    result.short_buffer_status = "PASS";

    std::vector<std::uint8_t> received(pattern.size());
    NozzleMappedPixels copied{};
    code = nozzle_frame_copy_pixels_with_origin(
        received_frame,
        NOZZLE_ORIGIN_TOP_LEFT,
        received.data(),
        received.size(),
        &copied);
    if (code != NOZZLE_OK) {
        result.message = error_message("nozzle_frame_copy_pixels_with_origin", code);
        release_frame(&received_frame);
        destroy_receiver(&receiver);
        destroy_sender(&sender);
        return result;
    }
    release_frame(&received_frame);
    result.copied_format = format_name(copied.format);

    const auto metadata_status = validate_copied_metadata(copied, width, height);
    if (metadata_status != "ok") {
        result.message = metadata_status;
        destroy_receiver(&receiver);
        destroy_sender(&sender);
        return result;
    }

    const auto received_rgba = storage_to_rgba(received, copied.format);
    const auto oracle =
        cinder::nozzle::assert_rgba_pattern(received_rgba, width, height);
    if (!oracle.ok()) {
        result.message = "oracle=" + oracle.message;
        destroy_receiver(&receiver);
        destroy_sender(&sender);
        return result;
    }

    result.pass = true;
    result.sender_status = "PASS";
    result.receiver_status = "PASS";
    result.copy_cost = "cpu-copy";
    result.message = "ok";
    destroy_receiver(&receiver);
    destroy_sender(&sender);
    return result;
}

void fail_now(const char *message, int code) {
    std::cerr << "CINDER_NOZZLE_RUNTIME_SMOKE FAIL message=" << message << "\n";
    std::exit(code);
}

} // namespace

class CinderNozzleRuntimeSmokeApp : public ci::app::App {
public:
    void setup() override {
        std::cout << "CINDER_NOZZLE_RUNTIME setup renderer=RendererGl width=" << getWindowWidth()
                  << " height=" << getWindowHeight() << "\n";
    }

    void draw() override {
        if (ran_) {
            quit();
            return;
        }
        ran_ = true;

        const bool has_context = ci::gl::context() != nullptr;
        std::cout << "CINDER_NOZZLE_GL_CONTEXT current=" << (has_context ? "PASS" : "FAIL") << "\n";
        if (!has_context) {
            fail_now("missing-current-gl-context", 2);
        }

        const auto diagnostics = cinder::nozzle::capture_diagnostics();
        std::cout << "CINDER_NOZZLE_DIAGNOSTICS " << diagnostics.summary() << "\n";
        if (diagnostics.cinder_target != "v0.9.3") {
            fail_now("unexpected-cinder-target", 3);
        }
        if (!diagnostics.cinder_gl_headers_available) {
            fail_now("missing-cinder-gl-headers", 4);
        }

        if (!exercise_pattern_oracle(320, 240)) {
            fail_now("cpu-oracle-320x240", 5);
        }
        if (!exercise_pattern_oracle(641, 479)) {
            fail_now("cpu-oracle-641x479", 6);
        }

        auto sender_texture = ci::gl::Texture2d::create(320, 240);
        auto receiver_texture = ci::gl::Texture2d::create(641, 479);
        cinder::nozzle::sender texture_sender{"cinder-runtime-smoke-texture"};
        cinder::nozzle::receiver texture_receiver{"cinder-runtime-smoke-texture"};
        const auto texture_sender_status = texture_sender.publish_texture(sender_texture);
        const auto texture_receiver_status = texture_receiver.try_update_texture(receiver_texture);
        if (texture_sender_status.code != cinder::nozzle::path_status::missing_host_smoke) {
            fail_now("unexpected-texture-sender-status", 7);
        }
        if (texture_receiver_status.code != cinder::nozzle::path_status::missing_host_smoke) {
            fail_now("unexpected-texture-receiver-status", 8);
        }
        texture_sender.stop();
        texture_receiver.stop();

        const auto interop_320 = exercise_nozzle_cpu_frame_interop(320, 240);
        std::cout << "CINDER_NOZZLE_FRAME_INTEROP size=320x240 frame_sender=" << interop_320.sender_status
                  << " frame_receiver=" << interop_320.receiver_status
                  << " mapped_format=" << interop_320.mapped_format
                  << " copied_format=" << interop_320.copied_format
                  << " short_buffer=" << interop_320.short_buffer_status
                  << " copy_cost=" << interop_320.copy_cost
                  << " detail=" << interop_320.message << "\n";
        if (!interop_320.pass) {
            fail_now("frame-interop-320x240", 9);
        }

        const auto interop_641 = exercise_nozzle_cpu_frame_interop(641, 479);
        std::cout << "CINDER_NOZZLE_FRAME_INTEROP size=641x479 frame_sender=" << interop_641.sender_status
                  << " frame_receiver=" << interop_641.receiver_status
                  << " mapped_format=" << interop_641.mapped_format
                  << " copied_format=" << interop_641.copied_format
                  << " short_buffer=" << interop_641.short_buffer_status
                  << " copy_cost=" << interop_641.copy_cost
                  << " detail=" << interop_641.message << "\n";
        if (!interop_641.pass) {
            fail_now("frame-interop-641x479", 10);
        }

        std::cout << "CINDER_NOZZLE_TEXTURE_TRANSFER texture_sender="
                  << cinder::nozzle::to_string(texture_sender_status.code)
                  << " texture_receiver=" << cinder::nozzle::to_string(texture_receiver_status.code)
                  << " texture_transfer=MISSING_HOST_SMOKE copy_cost=UNPROVEN\n";
        std::cout << "CINDER_NOZZLE_FRAME_PATH frame_sender=PASS frame_receiver=PASS copy_cost=cpu-copy\n";

        std::cout << "CINDER_NOZZLE_RUNTIME_SMOKE PASS\n";
        quit();
    }

private:
    bool ran_ = false;
};

CINDER_APP(CinderNozzleRuntimeSmokeApp, ci::app::RendererGl, [](ci::app::App::Settings *settings) {
    settings->setWindowSize(320, 240);
    settings->setTitle("CinderNozzleRuntimeSmoke");
})
