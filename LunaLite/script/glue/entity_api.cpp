#include "../../scene/components.h"
#include "../../scene/scene.h"
#include "entity_api.h"

namespace lunalite::script {
namespace {
glm::quat normalizedRotation(const glm::quat& rotation)
{
    if (glm::length(rotation) == 0.0f) {
        return glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
    }

    return glm::normalize(rotation);
}
} // namespace

uint32_t EntityAPI::getHandle(scene::Entity entity)
{
    return static_cast<uint32_t>(entity.getHandle());
}

bool EntityAPI::isValid(scene::Scene& scene, scene::Entity entity)
{
    return scene.isValidEntity(entity);
}

void EntityAPI::destroy(scene::Scene& scene, scene::Entity entity)
{
    scene.destroyEntity(entity);
}

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

    return glm::eulerAngles(scene.getComponent<scene::TransformComponent>(entity).rotation);
}

void EntityAPI::setRotation(scene::Scene& scene, scene::Entity entity, const glm::vec3& value)
{
    setRotationEuler(scene, entity, value);
}

void EntityAPI::setRotationEuler(scene::Scene& scene, scene::Entity entity, const glm::vec3& value)
{
    if (!scene.isValidEntity(entity) || !scene.hasComponent<scene::TransformComponent>(entity)) {
        return;
    }

    scene.getComponent<scene::TransformComponent>(entity).rotation = normalizedRotation(glm::quat{value});
}

void EntityAPI::setOrientationQuat(scene::Scene& scene, scene::Entity entity, const glm::quat& value)
{
    if (!scene.isValidEntity(entity) || !scene.hasComponent<scene::TransformComponent>(entity)) {
        return;
    }

    scene.getComponent<scene::TransformComponent>(entity).rotation = normalizedRotation(value);
}

void EntityAPI::setYawPitch(scene::Scene& scene, scene::Entity entity, float yaw, float pitch)
{
    const auto yawRotation = glm::angleAxis(yaw, glm::vec3{0.0f, 1.0f, 0.0f});
    const auto pitchRotation = glm::angleAxis(pitch, glm::vec3{1.0f, 0.0f, 0.0f});
    setOrientationQuat(scene, entity, yawRotation * pitchRotation);
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

bool EntityAPI::hasMesh(scene::Scene& scene, scene::Entity entity)
{
    return scene.isValidEntity(entity) && scene.hasComponent<scene::MeshComponent>(entity);
}

asset::AssetHandle EntityAPI::getMesh(scene::Scene& scene, scene::Entity entity)
{
    if (!hasMesh(scene, entity)) {
        return asset::AssetHandle{0};
    }

    return scene.getComponent<scene::MeshComponent>(entity).mesh;
}

void EntityAPI::setMesh(scene::Scene& scene, scene::Entity entity, asset::AssetHandle mesh)
{
    if (!scene.isValidEntity(entity)) {
        return;
    }

    if (!scene.hasComponent<scene::MeshComponent>(entity)) {
        scene.addComponent<scene::MeshComponent>(entity);
    }

    scene.getComponent<scene::MeshComponent>(entity).mesh = mesh;
}

void EntityAPI::removeMesh(scene::Scene& scene, scene::Entity entity)
{
    if (hasMesh(scene, entity)) {
        scene.removeComponent<scene::MeshComponent>(entity);
    }
}

bool EntityAPI::hasCamera(scene::Scene& scene, scene::Entity entity)
{
    return scene.isValidEntity(entity) && scene.hasComponent<scene::CameraComponent>(entity);
}

void EntityAPI::addCamera(scene::Scene& scene, scene::Entity entity)
{
    if (scene.isValidEntity(entity) && !scene.hasComponent<scene::CameraComponent>(entity)) {
        scene.addComponent<scene::CameraComponent>(entity);
    }
}

void EntityAPI::removeCamera(scene::Scene& scene, scene::Entity entity)
{
    if (hasCamera(scene, entity)) {
        scene.removeComponent<scene::CameraComponent>(entity);
    }
}

bool EntityAPI::hasDirectionalLight(scene::Scene& scene, scene::Entity entity)
{
    return scene.isValidEntity(entity) && scene.hasComponent<scene::DirectionalLightComponent>(entity);
}

void EntityAPI::addDirectionalLight(scene::Scene& scene, scene::Entity entity)
{
    if (scene.isValidEntity(entity) && !scene.hasComponent<scene::DirectionalLightComponent>(entity)) {
        scene.addComponent<scene::DirectionalLightComponent>(entity);
    }
}

void EntityAPI::removeDirectionalLight(scene::Scene& scene, scene::Entity entity)
{
    if (hasDirectionalLight(scene, entity)) {
        scene.removeComponent<scene::DirectionalLightComponent>(entity);
    }
}

} // namespace lunalite::script
