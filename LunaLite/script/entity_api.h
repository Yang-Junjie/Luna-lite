#pragma once
#include "../scene/entity.h"

#include <glm/glm.hpp>
#include <string>

namespace lunalite::scene {
class Scene;
}

namespace lunalite::script {
class EntityAPI {
public:
    static std::string getTag(scene::Scene& scene, scene::Entity entity);
    static void setTag(scene::Scene& scene, scene::Entity entity, const std::string& tag);

    static glm::vec3 getTranslation(scene::Scene& scene, scene::Entity entity);
    static void setTranslation(scene::Scene& scene, scene::Entity entity, const glm::vec3& value);

    static glm::vec3 getRotation(scene::Scene& scene, scene::Entity entity);
    static void setRotation(scene::Scene& scene, scene::Entity entity, const glm::vec3& value);

    static glm::vec3 getScale(scene::Scene& scene, scene::Entity entity);
    static void setScale(scene::Scene& scene, scene::Entity entity, const glm::vec3& value);
};
} // namespace lunalite::script
