#pragma once

#include <string>
#include "cinder/nozzle/Status.h"
#include "cinder/nozzle/TextureTypes.h"

namespace cinder::nozzle {

class receiver {
public:
    explicit receiver(std::string source_name);
    ~receiver();

    receiver(const receiver &) = delete;
    receiver &operator=(const receiver &) = delete;

    [[nodiscard]] const std::string &source_name() const;
    [[nodiscard]] status try_update_texture(const ci::gl::TextureRef &texture);
    void stop();

private:
    std::string source_name_;
    bool stopped_ = false;
};

} // namespace cinder::nozzle
