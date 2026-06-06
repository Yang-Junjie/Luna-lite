#pragma once

#include <cstdint>

namespace lunalite::renderer::interface {
struct FrameImage;
struct FrameRenderData;

class Renderer {
public:
    Renderer() = default;
    virtual ~Renderer() = default;

    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
    virtual void renderFrame(const FrameRenderData& frame) = 0;

    virtual const FrameImage& getFrameImage() const = 0;
};
} // namespace lunalite::renderer::interface
