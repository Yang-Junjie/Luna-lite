#include "component_registry.h"
#include "component_registry_adapters.h"
#include "scene_component_registry.h"
#include "scene_components.h"

namespace lunalite::scene {

void registerCoreSceneComponents(ComponentRegistry& registry)
{
    registry.registerComponent({
        .id = TagComponentType,
        .display_name = "Tag",
        .category = "Scene",
        .has = &componentHasAdapter<TagComponent>,
        .user_addable = false,
    });
    registry.registerComponent({
        .id = TransformComponentType,
        .display_name = "Transform",
        .category = "Scene",
        .has = &componentHasAdapter<TransformComponent>,
        .user_addable = false,
    });
    registry.registerComponent({
        .id = ParentComponentType,
        .display_name = "Parent",
        .category = "Scene",
        .has = &componentHasAdapter<ParentComponent>,
        .user_addable = false,
    });
}

} // namespace lunalite::scene
