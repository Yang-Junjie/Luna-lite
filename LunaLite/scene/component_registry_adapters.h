#pragma once

#include "component_registry.h"
#include "scene.h"

namespace lunalite::scene {

template <typename Component> bool componentHasAdapter(const Scene& scene, Entity entity)
{
    return scene.isValidEntity(entity) && scene.hasComponent<Component>(entity);
}

template <typename Component> bool componentAddAdapter(Scene& scene, Entity entity)
{
    if (!scene.isValidEntity(entity) || scene.hasComponent<Component>(entity)) {
        return false;
    }

    scene.addComponent<Component>(entity);
    return true;
}

template <typename Component> bool componentRemoveAdapter(Scene& scene, Entity entity)
{
    if (!scene.isValidEntity(entity) || !scene.hasComponent<Component>(entity)) {
        return false;
    }

    scene.removeComponent<Component>(entity);
    return true;
}

} // namespace lunalite::scene
