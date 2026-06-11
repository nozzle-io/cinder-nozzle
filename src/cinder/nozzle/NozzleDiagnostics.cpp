#include "cinder/nozzle/NozzleDiagnostics.h"
#include "cinder/nozzle/TextureTypes.h"
#include "nozzle/nozzle_c.h"

#if CINDER_NOZZLE_HAS_CINDER_GL
#include "cinder/gl/wrapper.h"
#endif

#include <sstream>

namespace cinder::nozzle {

diagnostics capture_diagnostics() {
    diagnostics out;
    out.cinder_gl_headers_available = CINDER_NOZZLE_HAS_CINDER_GL != 0;
#if CINDER_NOZZLE_HAS_CINDER_GL
    out.current_gl_context_proven = cinder::gl::context() != nullptr;
#else
    out.current_gl_context_proven = false;
#endif
    out.cinder_runtime_texture_path = path_status::missing_host_smoke;
    out.windows_fast_gpu_interop = path_status::unsupported;
    out.linux_gl_interop = path_status::unsupported;
    static_assert(NOZZLE_OK == 0, "unexpected nozzle C ABI NOZZLE_OK value");
    return out;
}

std::string diagnostics::summary() const {
    std::ostringstream ss;
    ss << "cinder=" << cinder_target
       << " tag=" << cinder_tag
       << " target=" << cinder_tag_target
       << " nozzle=" << nozzle_sha
       << " cinder_gl_headers=" << (cinder_gl_headers_available ? "yes" : "no")
       << " current_gl_context_proven=" << (current_gl_context_proven ? "yes" : "no")
       << " runtime_texture_path=" << to_string(cinder_runtime_texture_path)
       << " windows_fast_gpu=" << to_string(windows_fast_gpu_interop)
       << " linux_gl=" << to_string(linux_gl_interop);
    return ss.str();
}

} // namespace cinder::nozzle
