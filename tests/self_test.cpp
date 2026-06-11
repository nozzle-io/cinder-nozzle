#include "cinder/nozzle/CinderNozzle.h"
#include <algorithm>
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
        std::cerr << label << ": expected pass, got " << result.message << "\n";
        return false;
    }
    return true;
}

bool expect_fail(const std::vector<std::uint8_t> &rgba, std::uint32_t width, std::uint32_t height, const char *label) {
    const auto result = cinder::nozzle::assert_rgba_pattern(rgba, width, height);
    if (result.ok()) {
        std::cerr << label << ": expected fail\n";
        return false;
    }
    return true;
}

bool exercise_pattern_oracle(std::uint32_t width, std::uint32_t height) {
    const auto pattern = cinder::nozzle::make_rgba_pattern(width, height);
    if (!expect_pass(pattern, width, height, "positive oracle")) {
        return false;
    }
    if (!expect_fail(y_flipped(pattern, width, height), width, height, "y-flip negative oracle")) {
        return false;
    }
    if (!expect_fail(rb_swapped(pattern), width, height, "R/B swap negative oracle")) {
        return false;
    }
    if (!expect_fail(alpha_mutated(pattern), width, height, "alpha negative oracle")) {
        return false;
    }
    auto truncated = pattern;
    truncated.pop_back();
    if (!expect_fail(truncated, width, height, "byte-size negative oracle")) {
        return false;
    }
    return true;
}

} // namespace

int main() {
    const auto diagnostics = cinder::nozzle::capture_diagnostics();
    std::cout << diagnostics.summary() << "\n";
    if (diagnostics.cinder_target != "v0.9.3") {
        return 1;
    }
    if (!exercise_pattern_oracle(320, 240)) {
        return 2;
    }
    if (!exercise_pattern_oracle(641, 479)) {
        return 3;
    }
    if (cinder::nozzle::assert_rgba_pattern({}, 0, 240).ok()) {
        return 4;
    }
    if (!cinder::nozzle::make_rgba_pattern(0, 240).empty()) {
        return 5;
    }
    cinder::nozzle::sender sender{"test"};
    const auto publish_status = sender.publish_texture({}, cinder::nozzle::texture_format::rgba8_unorm);
    if (publish_status.code != cinder::nozzle::path_status::fail) {
        return 6;
    }
    std::cout << "CINDER_NOZZLE_SELF_TEST PASS\n";
    return 0;
}
