#pragma once

#include <string>
#include "cinder/nozzle/Status.h"

namespace cinder::nozzle {

struct diagnostics {
    std::string cinder_target = "v0.9.3";
    std::string cinder_tag = "221e15f04627ef5fb225a593cb0efa7be282d4f9";
    std::string cinder_tag_target = "70c2904643ac5978e439bd79ca64223169d366f6";
    std::string nozzle_sha = "a8efca3c847c39b76057a8e77f94b34146cc9125";
    bool cinder_gl_headers_available = false;
    bool current_gl_context_proven = false;
    path_status cinder_runtime_texture_path = path_status::missing_host_smoke;
    path_status windows_fast_gpu_interop = path_status::unsupported;
    path_status linux_gl_interop = path_status::unsupported;

    [[nodiscard]] std::string summary() const;
};

diagnostics capture_diagnostics();

} // namespace cinder::nozzle
