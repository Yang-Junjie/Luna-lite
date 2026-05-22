#include "scene.h"

namespace lunalite::scene {

Entity Scene::createEntity()
{
    return Entity{m_registry.create()};
}

} // namespace lunalite::scene
