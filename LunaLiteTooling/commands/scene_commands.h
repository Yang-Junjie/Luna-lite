#pragma once

#include <string_view>

namespace lunalite::tooling {

class CommandRegistry;

inline constexpr std::string_view CreateEntityCommandId = "scene.create_entity";
inline constexpr std::string_view DeleteEntityCommandId = "scene.delete_entity";
inline constexpr std::string_view SetParentCommandId = "scene.set_parent";
inline constexpr std::string_view ClearParentCommandId = "scene.clear_parent";
inline constexpr std::string_view CreateEntityFromAssetCommandId = "scene.create_entity_from_asset";

void registerSceneCommands(CommandRegistry& registry);

} // namespace lunalite::tooling
