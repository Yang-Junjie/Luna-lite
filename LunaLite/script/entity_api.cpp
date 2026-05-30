#include "../scene/components.h"
#include "../scene/scene.h"
#include "entity_api.h"

namespace lunalite::script {

std::string EntityAPI::getTag(scene::Scene& scene, scene::Entity entity)
{
    if (!scene.isValidEntity(entity) || !scene.hasComponent<scene::TagComponent>(entity)) {
        return {};
    }

    return scene.getComponent<scene::TagComponent>(entity).tag;
}

void EntityAPI::setTag(scene::Scene& scene, scene::Entity entity, const std::string& tag)
{
    if (!scene.isValidEntity(entity) || !scene.hasComponent<scene::TagComponent>(entity)) {
        return;
    }

    scene.getComponent<scene::TagComponent>(entity).tag = tag;
}

glm::vec3 EntityAPI::getTranslation(scene::Scene& scene, scene::Entity entity)
{
    if (!scene.isValidEntity(entity) || !scene.hasComponent<scene::TransformComponent>(entity)) {
        return {};
    }

    return scene.getComponent<scene::TransformComponent>(entity).translation;
}

void EntityAPI::setTranslation(scene::Scene& scene, scene::Entity entity, const glm::vec3& value)
{
    if (!scene.isValidEntity(entity) || !scene.hasComponent<scene::TransformComponent>(entity)) {
        return;
    }

    scene.getComponent<scene::TransformComponent>(entity).translation = value;
}

glm::vec3 EntityAPI::getRotation(scene::Scene& scene, scene::Entity entity)
{
    if (!scene.isValidEntity(entity) || !scene.hasComponent<scene::TransformComponent>(entity)) {
        return {};
    }

    return scene.getComponent<scene::TransformComponent>(entity).rotation;
}

void EntityAPI::setRotation(scene::Scene& scene, scene::Entity entity, const glm::vec3& value)
{
    if (!scene.isValidEntity(entity) || !scene.hasComponent<scene::TransformComponent>(entity)) {
        return;
    }

    scene.getComponent<scene::TransformComponent>(entity).rotation = value;
}

glm::vec3 EntityAPI::getScale(scene::Scene& scene, scene::Entity entity)
{
    if (!scene.isValidEntity(entity) || !scene.hasComponent<scene::TransformComponent>(entity)) {
        return glm::vec3{1.0f};
    }

    return scene.getComponent<scene::TransformComponent>(entity).scale;
}

void EntityAPI::setScale(scene::Scene& scene, scene::Entity entity, const glm::vec3& value)
{
    if (!scene.isValidEntity(entity) || !scene.hasComponent<scene::TransformComponent>(entity)) {
        return;
    }

    scene.getComponent<scene::TransformComponent>(entity).scale = value;
}

} // namespace lunalite::script
