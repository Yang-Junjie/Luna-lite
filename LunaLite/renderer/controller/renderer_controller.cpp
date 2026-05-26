#include "../default_renderer/renderer.h"
#include "../interface/renderer.h"
#include "../soft_rasterization_renderer/soft_rasterization_renderer.h"
#include "renderer_controller.h"

#include "TinyRHI/interface/device.h"
#include "TinyRHI/interface/swapchain.h"

namespace lunalite::renderer {

RendererController::RendererController(rhi::Device& device,
                                       rhi::Swapchain& swapchain,
                                       uint32_t width,
                                       uint32_t height,
                                       interface::RendererKind initial_kind)
    : m_device(device),
      m_swapchain(swapchain),
      m_width(width),
      m_height(height)
{
    switchRenderer(initial_kind);
}

RendererController::~RendererController() = default;

interface::Renderer& RendererController::getRenderer()
{
    return *m_renderer;
}

const interface::Renderer& RendererController::getRenderer() const
{
    return *m_renderer;
}

interface::RendererKind RendererController::getKind() const
{
    return m_kind;
}

void RendererController::switchRenderer(interface::RendererKind kind)
{
    if (m_renderer && m_kind == kind) {
        return;
    }

    m_renderer = createRenderer(kind);
    m_kind = kind;
}

void RendererController::resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) {
        return;
    }

    m_width = width;
    m_height = height;

    m_swapchain.resize(width, height);

    if (m_renderer) {
        m_renderer->resize(width, height);
    }
}

const interface::FrameImage& RendererController::getFrameImage() const
{
    return m_renderer->getFrameImage();
}

std::unique_ptr<interface::Renderer> RendererController::createRenderer(interface::RendererKind kind) const
{
    switch (kind) {
        case interface::RendererKind::Default:
            return std::make_unique<Renderer>(m_device, m_swapchain);
        case interface::RendererKind::SoftRasterization:
            return std::make_unique<SoftRasterizationRenderer>(m_width, m_height);
    }

    return std::make_unique<Renderer>(m_device, m_swapchain);
}

} // namespace lunalite::renderer
