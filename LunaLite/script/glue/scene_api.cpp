#include "../../scene/scene.h"
#include "entity_api.h"
#include "scene_api.h"

namespace lunalite::script {

scene::Entity SceneAPI::createEntity(scene::Scene& scene, const std::string& tag)
{
    auto entity = scene.createEntity();
    if (!tag.empty()) {
        EntityAPI::setTag(scene, entity, tag);
    }
    return entity;
}

void SceneAPI::destroyEntity(scene::Scene& scene, scene::Entity entity)
{
    scene.destroyEntity(entity);
}

scene::Entity SceneAPI::findEntityByTag(scene::Scene& scene, const std::string& tag)
{
    for (const auto entity : scene.getEntities()) {
        if (EntityAPI::getTag(scene, entity) == tag) {
            return entity;
        }
    }

    return scene::Entity{};
}

} // namespace lunalite::script
