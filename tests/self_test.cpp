#include "cinder/nozzle/CinderNozzle.h"
#include <iostream>

int main() {
    const auto diagnostics = cinder::nozzle::capture_diagnostics();
    std::cout << diagnostics.summary() << "\n";
    if (diagnostics.cinder_target != "v0.9.3") {
        return 1;
    }
    const auto pattern_a = cinder::nozzle::make_rgba_pattern(320, 240);
    const auto status_a = cinder::nozzle::assert_rgba_pattern(pattern_a, 320, 240);
    if (!status_a.ok()) {
        std::cerr << status_a.message << "\n";
        return 2;
    }
    const auto pattern_b = cinder::nozzle::make_rgba_pattern(641, 479);
    const auto status_b = cinder::nozzle::assert_rgba_pattern(pattern_b, 641, 479);
    if (!status_b.ok()) {
        std::cerr << status_b.message << "\n";
        return 3;
    }
    cinder::nozzle::sender sender{"test"};
    const auto publish_status = sender.publish_texture({}, cinder::nozzle::texture_format::rgba8_unorm);
    if (publish_status.code != cinder::nozzle::path_status::fail) {
        return 4;
    }
    std::cout << "CINDER_NOZZLE_SELF_TEST PASS\n";
    return 0;
}
