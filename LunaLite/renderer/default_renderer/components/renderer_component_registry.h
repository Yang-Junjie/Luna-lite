#pragma once

#include <string_view>

namespace lunalite::scene {
class ComponentRegistry;
}

namespace lunalite::renderer {

inline constexpr std::string_view MeshRendererComponentType = "MeshRenderer";
inline constexpr std::string_view SpriteRendererComponentType = "SpriteRenderer";
inline constexpr std::string_view CameraComponentType = "Camera";
inline constexpr std::string_view DirectionalLightComponentType = "DirectionalLight";

void registerRendererComponents(scene::ComponentRegistry& registry);

} // namespace lunalite::renderer
