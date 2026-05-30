#include "components.h"
#include "scene.h"

namespace lunalite::scene {

Entity Scene::createEntity()
{
    Entity entity{m_registry.create()};
    addComponent<TagComponent>(entity);
    addComponent<TransformComponent>(entity);
    return entity;
}

void Scene::destroyEntity(Entity entity)
{
    if (isValidEntity(entity)) {
        m_registry.destroy(entity.getHandle());
    }
}

void Scene::clear()
{
    m_registry.clear();
}

void Scene::copyFrom(const Scene& other)
{
    clear();

    for (const auto sourceEntity : other.getEntities()) {
        auto targetEntity = createEntity();

        if (other.hasComponent<TagComponent>(sourceEntity)) {
            getComponent<TagComponent>(targetEntity) = other.getComponent<TagComponent>(sourceEntity);
        }

        if (other.hasComponent<TransformComponent>(sourceEntity)) {
            getComponent<TransformComponent>(targetEntity) = other.getComponent<TransformComponent>(sourceEntity);
        }

        if (other.hasComponent<MeshComponent>(sourceEntity)) {
            addComponent<MeshComponent>(targetEntity) = other.getComponent<MeshComponent>(sourceEntity);
        }

        if (other.hasComponent<CameraComponent>(sourceEntity)) {
            addComponent<CameraComponent>(targetEntity) = other.getComponent<CameraComponent>(sourceEntity);
        }

        if (other.hasComponent<DirectionalLightComponent>(sourceEntity)) {
            addComponent<DirectionalLightComponent>(targetEntity) =
                other.getComponent<DirectionalLightComponent>(sourceEntity);
        }
    }
}

void Scene::onRuntimeStart() {}

void Scene::onUpdateEditor(core::Timestep dt)
{
    (void) dt;
}

void Scene::onUpdateRuntime(core::Timestep dt)
{
    (void) dt;
}

void Scene::onRuntimeStop() {}

bool Scene::isValidEntity(Entity entity) const
{
    return m_registry.valid(entity.getHandle());
}

std::vector<Entity> Scene::getEntities() const
{
    std::vector<Entity> entities;
    const auto* storage = m_registry.storage<entt::entity>();
    entities.reserve(storage->size());

    for (const auto [handle] : storage->each()) {
        entities.emplace_back(handle);
    }

    return entities;
}

} // namespace lunalite::scene
