#pragma once

#include <string>

namespace cinder::nozzle {

enum class path_status {
    pass,
    fail,
    missing_host_smoke,
    unsupported
};

struct status {
    path_status code = path_status::pass;
    std::string message;

    [[nodiscard]] bool ok() const { return code == path_status::pass; }
};

const char *to_string(path_status value);

} // namespace cinder::nozzle
