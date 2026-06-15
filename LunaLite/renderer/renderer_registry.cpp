#include "interface/renderer.h"
#include "renderer_registry.h"

#include <algorithm>
#include <utility>

namespace lunalite::renderer {

RendererRegistry& RendererRegistry::get()
{
    static RendererRegistry registry;
    return registry;
}

bool RendererRegistry::registerRenderer(RendererDescriptor descriptor)
{
    if (!descriptor.create || descriptor.display_name.empty() || find(descriptor.kind) != nullptr) {
        return false;
    }

    m_renderers.push_back(std::move(descriptor));
    return true;
}

const RendererDescriptor* RendererRegistry::find(interface::RendererKind kind) const
{
    const auto it = std::ranges::find_if(m_renderers, [kind](const RendererDescriptor& descriptor) {
        return descriptor.kind == kind;
    });
    return it != m_renderers.end() ? &*it : nullptr;
}

std::span<const RendererDescriptor> RendererRegistry::renderers() const
{
    return m_renderers;
}

std::unique_ptr<interface::Renderer> RendererRegistry::create(
    interface::RendererKind kind, rhi::Device& device, rhi::Swapchain& swapchain, uint32_t width, uint32_t height) const
{
    const auto* descriptor = find(kind);
    if (descriptor == nullptr || !descriptor->create) {
        return nullptr;
    }

    return descriptor->create(RendererCreateInfo{
        .device = device,
        .swapchain = swapchain,
        .width = width,
        .height = height,
    });
}

} // namespace lunalite::renderer
