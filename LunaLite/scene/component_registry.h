#pragma once

#include "entity.h"

#include <span>
#include <string_view>
#include <vector>

namespace lunalite::scene {

class Scene;

using ComponentHasFn = bool (*)(const Scene& scene, Entity entity);
using ComponentEditFn = bool (*)(Scene& scene, Entity entity);
using ComponentInspectFn = bool (*)(Scene& scene, Entity entity);

struct ComponentDescriptor {
    std::string_view id;
    std::string_view display_name;
    std::string_view category;
    ComponentHasFn has{nullptr};
    ComponentEditFn add{nullptr};
    ComponentEditFn remove{nullptr};
    ComponentInspectFn inspect{nullptr};
    bool user_addable{true};
};

class ComponentRegistry {
public:
    static ComponentRegistry& get();

    bool registerComponent(ComponentDescriptor descriptor);
    bool setInspector(std::string_view id, ComponentInspectFn inspect);
    ComponentDescriptor* find(std::string_view id);
    const ComponentDescriptor* find(std::string_view id) const;
    std::span<const ComponentDescriptor> components() const;

private:
    std::vector<ComponentDescriptor> m_components;
};

void registerSceneComponents(ComponentRegistry& registry);

} // namespace lunalite::scene
