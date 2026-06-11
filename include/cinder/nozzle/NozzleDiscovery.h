#pragma once

#include <string>
#include <vector>

namespace cinder::nozzle {

struct source_info {
    std::string name;
    std::string application_name;
};

std::vector<source_info> list_sources();

} // namespace cinder::nozzle
