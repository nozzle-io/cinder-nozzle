#pragma once

#include <memory>

#if __has_include("cinder/gl/Texture.h")
#include "cinder/gl/Texture.h"
#define CINDER_NOZZLE_HAS_CINDER_GL 1
#else
#define CINDER_NOZZLE_HAS_CINDER_GL 0
namespace cinder { namespace gl { class Texture2d; using TextureRef = std::shared_ptr<Texture2d>; } }
namespace ci = cinder;
#endif
