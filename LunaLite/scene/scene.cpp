#include "scene.h"

namespace lunalite::scene {

Entity Scene::createEntity()
{
    return Entity{m_registry.create()};
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
