#include "../renderer/default_renderer/components/renderer_component_registry.h"
#include "../renderer/default_renderer/renderer.h"
#include "../renderer/renderer_registry.h"
#include "../scene/component_registry.h"
#include "../script/script_component_registry.h"
#include "default_engine_modules.h"

#include <memory>

namespace lunalite::modules {
namespace {

std::unique_ptr<renderer::interface::Renderer> createDefaultRenderer(const renderer::RendererCreateInfo& info)
{
    return std::make_unique<renderer::Renderer>(info.device, info.swapchain);
}

void registerDefaultRenderer(renderer::RendererRegistry& renderer_registry)
{
    renderer_registry.registerRenderer({
        .kind = renderer::interface::RendererKind::Default,
        .display_name = "Default",
        .create = &createDefaultRenderer,
    });
}

} // namespace

void registerDefaultEngineModules(scene::ComponentRegistry& component_registry,
                                  renderer::RendererRegistry& renderer_registry)
{
    renderer::registerRendererComponents(component_registry);
    script::registerScriptComponents(component_registry);
    registerDefaultRenderer(renderer_registry);
}

void registerDefaultEngineModules(scene::ComponentRegistry& component_registry)
{
    registerDefaultEngineModules(component_registry, renderer::RendererRegistry::get());
}

void registerDefaultEngineModules()
{
    registerDefaultEngineModules(scene::ComponentRegistry::get(), renderer::RendererRegistry::get());
}

} // namespace lunalite::modules
