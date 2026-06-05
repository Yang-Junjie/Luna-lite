#pragma once
#include "../asset/asset.h"
#include "../core/timestep.h"
#include "../script/script_runtime.h"
#include "entity.h"

#include <glm/mat4x4.hpp>
#include <entt/entt.hpp>
#include <memory>
#include <utility>
#include <vector>

namespace lunalite::asset {
class Prefab;
}

namespace lunalite::scene {
struct SceneSettings {
    asset::AssetHandle environment_map{0};
    float environment_intensity{1.0f};
};

class Scene {
public:
    Scene() = default;

    Entity createEntity();
    void destroyEntity(Entity entity);
    bool setParent(Entity child, Entity parent, bool keepWorldTransform = true);
    bool clearParent(Entity child, bool keepWorldTransform = true);
    std::vector<Entity> instantiatePrefab(asset::AssetHandle prefab, Entity parent = {});

    void clear();
    void copyFrom(const Scene& other);

    void onRuntimeStart();
    void onUpdateEditor(core::Timestep dt);
    void onUpdateRuntime(core::Timestep dt);
    void onRuntimeStop();

    bool isValidEntity(Entity entity) const;

    std::vector<Entity> getEntities() const;
    std::vector<Entity> getRootEntities() const;
    std::vector<Entity> getChildren(Entity entity) const;
    Entity getParent(Entity entity) const;
    bool isAncestor(Entity possibleAncestor, Entity entity) const;
    glm::mat4 getWorldTransform(Entity entity) const;

    SceneSettings& getSettings()
    {
        return m_settings;
    }

    const SceneSettings& getSettings() const
    {
        return m_settings;
    }

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
    SceneSettings m_settings;
    entt::registry m_registry;
    std::unique_ptr<script::ScriptRuntime> m_script_runtime;
};
} // namespace lunalite::scene
