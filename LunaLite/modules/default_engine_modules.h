#pragma once

namespace lunalite::scene {
class ComponentRegistry;
}

namespace lunalite::renderer {
class RendererRegistry;
}

namespace lunalite::modules {

void registerDefaultEngineModules(scene::ComponentRegistry& component_registry,
                                  renderer::RendererRegistry& renderer_registry);
void registerDefaultEngineModules(scene::ComponentRegistry& component_registry);
void registerDefaultEngineModules();

} // namespace lunalite::modules
