#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/nozzle/CinderNozzle.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
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
        cinder::nozzle::sender sender{"cinder-runtime-smoke"};
        cinder::nozzle::receiver receiver{"cinder-runtime-smoke"};
        const auto sender_status = sender.publish_texture(sender_texture);
        const auto receiver_status = receiver.try_update_texture(receiver_texture);
        std::cout << "CINDER_NOZZLE_PATH_STATUS sender=" << cinder::nozzle::to_string(sender_status.code)
                  << " receiver=" << cinder::nozzle::to_string(receiver_status.code)
                  << " texture_transfer=MISSING_HOST_SMOKE copy_cost=UNPROVEN\n";
        if (sender_status.code != cinder::nozzle::path_status::missing_host_smoke) {
            fail_now("unexpected-sender-status", 7);
        }
        if (receiver_status.code != cinder::nozzle::path_status::missing_host_smoke) {
            fail_now("unexpected-receiver-status", 8);
        }
        sender.stop();
        receiver.stop();

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
