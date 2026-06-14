#pragma once

#include <string_view>

namespace lunalite::scene {

class ComponentRegistry;

inline constexpr std::string_view TagComponentType = "Tag";
inline constexpr std::string_view TransformComponentType = "Transform";
inline constexpr std::string_view ParentComponentType = "Parent";

void registerCoreSceneComponents(ComponentRegistry& registry);

} // namespace lunalite::scene
