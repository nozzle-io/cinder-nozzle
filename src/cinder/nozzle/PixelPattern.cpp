#include "cinder/nozzle/PixelPattern.h"
#include "cinder/nozzle/Status.h"

#include <array>
#include <stdexcept>

namespace cinder::nozzle {

std::vector<std::uint8_t> make_rgba_pattern(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument("width/height must be positive");
    }
    std::vector<std::uint8_t> out(static_cast<std::size_t>(width) * height * 4u);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto base = (static_cast<std::size_t>(y) * width + x) * 4u;
            out[base + 0] = static_cast<std::uint8_t>(x == 0 ? 255 : (x == width - 1 ? 32 : ((x * 251u + y * 17u) & 0xffu)));
            out[base + 1] = static_cast<std::uint8_t>(y == 0 ? 64 : (y == height - 1 ? 255 : ((x * 19u + y * 241u) & 0xffu)));
            out[base + 2] = static_cast<std::uint8_t>((x == width / 2u && y == height / 2u) ? 255 : ((x * 73u + y * 37u) & 0xffu));
            const auto alpha_case = (x + y) % 3u;
            out[base + 3] = static_cast<std::uint8_t>(alpha_case == 0 ? 0 : (alpha_case == 1 ? 128 : 255));
        }
    }
    return out;
}

status assert_rgba_pattern(const std::vector<std::uint8_t> &rgba, std::uint32_t width, std::uint32_t height) {
    const auto expected = make_rgba_pattern(width, height);
    if (rgba.size() != expected.size()) {
        return {path_status::fail, "rgba byte size mismatch"};
    }
    const std::array<std::size_t, 5> probes = {
        0,
        static_cast<std::size_t>(width - 1u),
        static_cast<std::size_t>(height - 1u) * width,
        static_cast<std::size_t>(height) * width - 1u,
        static_cast<std::size_t>(height / 2u) * width + (width / 2u)
    };
    for (auto pixel : probes) {
        const auto base = pixel * 4u;
        for (std::size_t c = 0; c < 4u; ++c) {
            if (rgba[base + c] != expected[base + c]) {
                return {path_status::fail, "rgba probe mismatch"};
            }
        }
    }
    return {path_status::pass, "cpu rgba oracle pass"};
}

} // namespace cinder::nozzle
