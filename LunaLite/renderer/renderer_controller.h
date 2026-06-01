#pragma once

#include "interface/frame_image.h"
#include "interface/renderer_kind.h"

#include <cstdint>

#include <memory>

namespace lunalite::rhi {
class Device;
class Swapchain;
} // namespace lunalite::rhi

namespace lunalite::renderer::interface {
class Renderer;
}

namespace lunalite::renderer {

class RendererController {
public:
    RendererController(rhi::Device& device,
                       rhi::Swapchain& swapchain,
                       uint32_t width,
                       uint32_t height,
                       interface::RendererKind initial_kind);
    ~RendererController();

    interface::Renderer& getRenderer();
    const interface::Renderer& getRenderer() const;
    interface::RendererKind getKind() const;
    void switchRenderer(interface::RendererKind kind);
    void resize(uint32_t width, uint32_t height);
    const interface::FrameImage& getFrameImage() const;

private:
    std::unique_ptr<interface::Renderer> createRenderer(interface::RendererKind kind) const;

    rhi::Device& m_device;
    rhi::Swapchain& m_swapchain;
    uint32_t m_width{0};
    uint32_t m_height{0};
    interface::RendererKind m_kind{interface::RendererKind::Default};
    std::unique_ptr<interface::Renderer> m_renderer;
};

} // namespace lunalite::renderer
