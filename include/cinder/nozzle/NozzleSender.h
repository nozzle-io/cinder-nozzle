#pragma once

#include <string>
#include "cinder/nozzle/Status.h"
#include "cinder/nozzle/TextureTypes.h"

namespace cinder::nozzle {

enum class texture_format {
    rgba8_unorm,
    bgra8_unorm
};

class sender {
public:
    explicit sender(std::string name);
    ~sender();

    sender(const sender &) = delete;
    sender &operator=(const sender &) = delete;

    [[nodiscard]] const std::string &name() const;
    [[nodiscard]] status publish_texture(const ci::gl::TextureRef &texture, texture_format format = texture_format::rgba8_unorm);
    void stop();

private:
    std::string name_;
    bool stopped_ = false;
};

} // namespace cinder::nozzle
