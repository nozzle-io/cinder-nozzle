#pragma once

#include "cinder/nozzle/Status.h"

#include <cstdint>
#include <vector>

namespace cinder::nozzle {

std::vector<std::uint8_t> make_rgba_pattern(std::uint32_t width, std::uint32_t height);
status assert_rgba_pattern(const std::vector<std::uint8_t> &rgba, std::uint32_t width, std::uint32_t height);

} // namespace cinder::nozzle
