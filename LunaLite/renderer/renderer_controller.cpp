#include "../core/log.h"
#include "default_renderer/renderer.h"
#include "interface/renderer.h"
#include "renderer_controller.h"
#include "soft_rasterization_renderer/soft_rasterization_renderer.h"
#include "TinyRHI/interface/device.h"
#include "TinyRHI/interface/swapchain.h"

namespace lunalite::renderer {
namespace {

const char* rendererKindName(interface::RendererKind kind)
{
    switch (kind) {
        case interface::RendererKind::Default:
            return "Default";
        case interface::RendererKind::SoftRasterization:
            return "SoftRasterization";
    }

    return "Unknown";
}

} // namespace

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
    LUNA_ASSERT(m_renderer, "Failed to create initial renderer.");
}

RendererController::~RendererController() = default;

interface::Renderer& RendererController::getRenderer()
{
    LUNA_ASSERT(m_renderer, "Renderer is null.");
    return *m_renderer;
}

const interface::Renderer& RendererController::getRenderer() const
{
    LUNA_ASSERT(m_renderer, "Renderer is null.");
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
    LUNA_ASSERT(m_renderer, "Failed to create renderer.");
    m_kind = kind;
    LUNA_CORE_INFO("Renderer switched to {}", rendererKindName(kind));
}

void RendererController::resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) {
        LUNA_CORE_DEBUG("Ignoring zero-sized renderer resize: {}x{}", width, height);
        return;
    }

    m_width = width;
    m_height = height;
    LUNA_CORE_DEBUG("Resizing renderer and swapchain to {}x{}", width, height);

    m_swapchain.resize(width, height);

    if (m_renderer) {
        m_renderer->resize(width, height);
    }
}

const interface::FrameImage& RendererController::getFrameImage() const
{
    LUNA_ASSERT(m_renderer, "Renderer is null.");
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
