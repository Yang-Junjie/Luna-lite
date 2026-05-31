#pragma once
#include "../../asset/asset.h"
#include "../../scene/entity.h"

#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace lunalite::scene {
class Scene;
}

namespace lunalite::script {
class EntityAPI {
public:
    static uint32_t getHandle(scene::Entity entity);
    static bool isValid(scene::Scene& scene, scene::Entity entity);
    static void destroy(scene::Scene& scene, scene::Entity entity);

    static std::string getTag(scene::Scene& scene, scene::Entity entity);
    static void setTag(scene::Scene& scene, scene::Entity entity, const std::string& tag);

    static glm::vec3 getTranslation(scene::Scene& scene, scene::Entity entity);
    static void setTranslation(scene::Scene& scene, scene::Entity entity, const glm::vec3& value);

    static glm::vec3 getRotation(scene::Scene& scene, scene::Entity entity);
    static void setRotation(scene::Scene& scene, scene::Entity entity, const glm::vec3& value);
    static void setRotationEuler(scene::Scene& scene, scene::Entity entity, const glm::vec3& value);
    static void setOrientationQuat(scene::Scene& scene, scene::Entity entity, const glm::quat& value);
    static void setYawPitch(scene::Scene& scene, scene::Entity entity, float yaw, float pitch);

    static glm::vec3 getScale(scene::Scene& scene, scene::Entity entity);
    static void setScale(scene::Scene& scene, scene::Entity entity, const glm::vec3& value);

    static bool hasMesh(scene::Scene& scene, scene::Entity entity);
    static asset::AssetHandle getMesh(scene::Scene& scene, scene::Entity entity);
    static void setMesh(scene::Scene& scene, scene::Entity entity, asset::AssetHandle mesh);
    static void removeMesh(scene::Scene& scene, scene::Entity entity);

    static bool hasCamera(scene::Scene& scene, scene::Entity entity);
    static void addCamera(scene::Scene& scene, scene::Entity entity);
    static void removeCamera(scene::Scene& scene, scene::Entity entity);

    static bool hasDirectionalLight(scene::Scene& scene, scene::Entity entity);
    static void addDirectionalLight(scene::Scene& scene, scene::Entity entity);
    static void removeDirectionalLight(scene::Scene& scene, scene::Entity entity);
};
} // namespace lunalite::script
