#include "component_registry.h"
#include "scene_component_registry.h"

#include <algorithm>
#include <string_view>

namespace lunalite::scene {
ComponentRegistry& ComponentRegistry::get()
{
    static ComponentRegistry registry;
    static const bool initialized = []() {
        registerCoreSceneComponents(registry);
        return true;
    }();
    (void) initialized;
    return registry;
}

bool ComponentRegistry::registerComponent(ComponentDescriptor descriptor)
{
    if (descriptor.id.empty() || find(descriptor.id) != nullptr) {
        return false;
    }

    m_components.push_back(descriptor);
    return true;
}

bool ComponentRegistry::setInspector(std::string_view id, ComponentInspectFn inspect)
{
    auto* descriptor = find(id);
    if (descriptor == nullptr) {
        return false;
    }

    descriptor->inspect = inspect;
    return true;
}

ComponentDescriptor* ComponentRegistry::find(std::string_view id)
{
    const auto it = std::ranges::find_if(m_components, [id](const ComponentDescriptor& descriptor) {
        return descriptor.id == id;
    });
    return it != m_components.end() ? &*it : nullptr;
}

const ComponentDescriptor* ComponentRegistry::find(std::string_view id) const
{
    const auto it = std::ranges::find_if(m_components, [id](const ComponentDescriptor& descriptor) {
        return descriptor.id == id;
    });
    return it != m_components.end() ? &*it : nullptr;
}

std::span<const ComponentDescriptor> ComponentRegistry::components() const
{
    return m_components;
}

void registerSceneComponents(ComponentRegistry& registry)
{
    registerCoreSceneComponents(registry);
}

} // namespace lunalite::scene
