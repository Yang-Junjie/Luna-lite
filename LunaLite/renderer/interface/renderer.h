#pragma once
#include "triangle.h"

#include <cstdint>

namespace lunalite::renderer::interface {
class Renderer {
public:
    Renderer() = default;
    virtual ~Renderer() = default;

    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void renderTriangle(const Triangle& triangle) = 0;
};
} // namespace lunalite::renderer::interface
