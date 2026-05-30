#pragma once
#include "../core/timestep.h"
#include "entity.h"

#include <entt/entt.hpp>
#include <utility>
#include <vector>

namespace lunalite::scene {
class Scene {
public:
    Scene() = default;

    Entity createEntity();
    void destroyEntity(Entity entity);
    void clear();
    void copyFrom(const Scene& other);
    void onRuntimeStart();
    void onUpdateEditor(core::Timestep dt);
    void onUpdateRuntime(core::Timestep dt);
    void onRuntimeStop();
    bool isValidEntity(Entity entity) const;
    std::vector<Entity> getEntities() const;

    template <typename T, typename... Args> T& addComponent(Entity entity, Args&&... args)
    {
        return m_registry.emplace<T>(entity.getHandle(), std::forward<Args>(args)...);
    }

    template <typename T> T& getComponent(Entity entity)
    {
        return m_registry.get<T>(entity.getHandle());
    }

    template <typename T> const T& getComponent(Entity entity) const
    {
        return m_registry.get<T>(entity.getHandle());
    }

    template <typename T> bool hasComponent(Entity entity) const
    {
        return m_registry.all_of<T>(entity.getHandle());
    }

    template <typename T> void removeComponent(Entity entity)
    {
        m_registry.remove<T>(entity.getHandle());
    }

    entt::registry& getRegistry()
    {
        return m_registry;
    }

    const entt::registry& getRegistry() const
    {
        return m_registry;
    }

private:
    entt::registry m_registry;
};
} // namespace lunalite::scene
