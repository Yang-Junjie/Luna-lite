#pragma once
#include "../../scene/entity.h"

#include <string>

namespace lunalite::scene {
class Scene;
}

namespace lunalite::script {
class SceneAPI {
public:
    static scene::Entity createEntity(scene::Scene& scene, const std::string& tag = {});
    static void destroyEntity(scene::Scene& scene, scene::Entity entity);
    static scene::Entity findEntityByTag(scene::Scene& scene, const std::string& tag);
};
} // namespace lunalite::script
