#include "cinder/nozzle/Status.h"

namespace cinder::nozzle {

const char *to_string(path_status value) {
    switch (value) {
        case path_status::pass: return "PASS";
        case path_status::fail: return "FAIL";
        case path_status::missing_host_smoke: return "MISSING_HOST_SMOKE";
        case path_status::unsupported: return "UNSUPPORTED";
    }
    return "UNKNOWN";
}

} // namespace cinder::nozzle
