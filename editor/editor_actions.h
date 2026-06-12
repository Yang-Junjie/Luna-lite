#pragma once

#include "../LunaLite/asset/asset.h"
#include "../LunaLite/asset/asset_types.h"
#include "../LunaLite/scene/scene.h"
#include "../LunaLiteTooling/commands/command_args.h"
#include "../LunaLiteTooling/commands/command_result.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace lunalite::editor::actions {

inline constexpr std::string_view EditTagCommandId = "scene.edit_tag";
inline constexpr std::string_view EditTransformCommandId = "scene.edit_transform";
inline constexpr std::string_view EditMeshRendererCommandId = "scene.edit_mesh_renderer";
inline constexpr std::string_view EditSpriteRendererCommandId = "scene.edit_sprite_renderer";
inline constexpr std::string_view EditScriptCommandId = "scene.edit_script";
inline constexpr std::string_view EditCameraCommandId = "scene.edit_camera";
inline constexpr std::string_view EditDirectionalLightCommandId = "scene.edit_directional_light";
inline constexpr std::string_view EditSceneSettingsCommandId = "scene.edit_scene_settings";

tooling::CommandResult
    executeSceneCommand(scene::Scene& scene, std::string_view commandId, const tooling::CommandArgs& args = {});
tooling::CommandResult executeAssetCommand(std::string_view commandId,
                                           const asset::AssetHandle& sourceAsset,
                                           const std::filesystem::path& targetDirectory);

std::optional<scene::Entity> entityFromCommandResult(const tooling::CommandResult& result);

std::optional<scene::Entity> createEntity(scene::Scene& scene, std::string name = {}, scene::Entity parent = {});
bool deleteEntity(scene::Scene& scene, scene::Entity entity);
bool setParent(scene::Scene& scene, scene::Entity entity, scene::Entity parent, bool keepWorldTransform);
bool clearParent(scene::Scene& scene, scene::Entity entity, bool keepWorldTransform);
std::optional<scene::Entity> createEntityFromAsset(scene::Scene& scene,
                                                   asset::AssetHandle sourceAsset,
                                                   asset::AssetType type,
                                                   scene::Entity targetEntity = {});

bool beginSceneEdit(scene::Scene& scene, std::string_view commandId);
bool commitSceneEdit(scene::Scene& scene);
bool cancelSceneEdit(scene::Scene& scene);
bool hasActiveSceneEdit();

} // namespace lunalite::editor::actions
