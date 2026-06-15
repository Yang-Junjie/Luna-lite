#pragma once

#include "interface/renderer_kind.h"

#include <cstdint>

#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace lunalite::rhi {
class Device;
class Swapchain;
} // namespace lunalite::rhi

namespace lunalite::renderer::interface {
class Renderer;
}

namespace lunalite::renderer {

struct RendererCreateInfo {
    rhi::Device& device;
    rhi::Swapchain& swapchain;
    uint32_t width{0};
    uint32_t height{0};
};

using RendererFactoryFn = std::function<std::unique_ptr<interface::Renderer>(const RendererCreateInfo&)>;

struct RendererDescriptor {
    interface::RendererKind kind{interface::RendererKind::Default};
    std::string_view display_name;
    RendererFactoryFn create;
};

class RendererRegistry {
public:
    static RendererRegistry& get();

    bool registerRenderer(RendererDescriptor descriptor);
    const RendererDescriptor* find(interface::RendererKind kind) const;
    std::span<const RendererDescriptor> renderers() const;

    std::unique_ptr<interface::Renderer> create(interface::RendererKind kind,
                                                rhi::Device& device,
                                                rhi::Swapchain& swapchain,
                                                uint32_t width,
                                                uint32_t height) const;

private:
    std::vector<RendererDescriptor> m_renderers;
};

} // namespace lunalite::renderer
