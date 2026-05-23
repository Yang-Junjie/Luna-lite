#pragma once

#include "../interface/frame_image.h"
#include "../interface/renderer_kind.h"

#include <cstdint>
#include <memory>

namespace lunalite::rhi {
class Instance;
}

namespace lunalite::renderer::interface {
class Renderer;
}

namespace lunalite::renderer {

class RendererController {
public:
    RendererController(rhi::Instance& rhi, uint32_t width, uint32_t height, interface::RendererKind initial_kind);
    ~RendererController();

    interface::Renderer& getRenderer();
    const interface::Renderer& getRenderer() const;
    interface::RendererKind getKind() const;
    void switchRenderer(interface::RendererKind kind);
    const interface::FrameImage& getFrameImage() const;

private:
    std::unique_ptr<interface::Renderer> createRenderer(interface::RendererKind kind) const;

    rhi::Instance& m_rhi;
    uint32_t m_width{0};
    uint32_t m_height{0};
    interface::RendererKind m_kind{interface::RendererKind::Default};
    std::unique_ptr<interface::Renderer> m_renderer;
};

} // namespace lunalite::renderer
