#include "../../scene/component_registry.h"
#include "../../scene/component_registry_adapters.h"
#include "renderer_component_registry.h"
#include "renderer_components.h"

namespace lunalite::renderer {

void registerRendererComponents(scene::ComponentRegistry& registry)
{
    registry.registerComponent({
        .id = MeshRendererComponentType,
        .display_name = "Mesh Renderer",
        .category = "Renderer",
        .has = &scene::componentHasAdapter<scene::MeshRendererComponent>,
        .add = &scene::componentAddAdapter<scene::MeshRendererComponent>,
        .remove = &scene::componentRemoveAdapter<scene::MeshRendererComponent>,
    });
    registry.registerComponent({
        .id = SpriteRendererComponentType,
        .display_name = "Sprite Renderer",
        .category = "Renderer",
        .has = &scene::componentHasAdapter<scene::SpriteRendererComponent>,
        .add = &scene::componentAddAdapter<scene::SpriteRendererComponent>,
        .remove = &scene::componentRemoveAdapter<scene::SpriteRendererComponent>,
    });
    registry.registerComponent({
        .id = CameraComponentType,
        .display_name = "Camera",
        .category = "Renderer",
        .has = &scene::componentHasAdapter<scene::CameraComponent>,
        .add = &scene::componentAddAdapter<scene::CameraComponent>,
        .remove = &scene::componentRemoveAdapter<scene::CameraComponent>,
    });
    registry.registerComponent({
        .id = DirectionalLightComponentType,
        .display_name = "Directional Light",
        .category = "Renderer",
        .has = &scene::componentHasAdapter<scene::DirectionalLightComponent>,
        .add = &scene::componentAddAdapter<scene::DirectionalLightComponent>,
        .remove = &scene::componentRemoveAdapter<scene::DirectionalLightComponent>,
    });
}

} // namespace lunalite::renderer
