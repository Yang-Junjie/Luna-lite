#pragma once

#include "../LunaLite/asset/asset.h"
#include "../LunaLite/asset/asset_types.h"
#include "../LunaLite/asset/sprite.h"
#include "../LunaLite/renderer/interface/material.h"
#include "../LunaLite/renderer/interface/texture.h"
#include "../LunaLite/scene/scene.h"
#include "../LunaLiteTooling/commands/command_args.h"
#include "../LunaLiteTooling/commands/command_result.h"
#include "../LunaLiteTooling/commands/scene_commands.h"

#include <cstddef>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace lunalite::editor::actions {

inline constexpr std::string_view EditTagCommandId = tooling::EditTagCommandId;
inline constexpr std::string_view EditTransformCommandId = tooling::EditTransformCommandId;
inline constexpr std::string_view EditMeshRendererCommandId = tooling::EditMeshRendererCommandId;
inline constexpr std::string_view EditSpriteRendererCommandId = tooling::EditSpriteRendererCommandId;
inline constexpr std::string_view EditScriptCommandId = tooling::EditScriptCommandId;
inline constexpr std::string_view EditCameraCommandId = tooling::EditCameraCommandId;
inline constexpr std::string_view EditDirectionalLightCommandId = tooling::EditDirectionalLightCommandId;
inline constexpr std::string_view EditSceneSettingsCommandId = tooling::EditSceneSettingsCommandId;

tooling::CommandResult
    executeSceneCommand(scene::Scene& scene, std::string_view commandId, const tooling::CommandArgs& args = {});
tooling::CommandResult executeAssetCommand(std::string_view commandId, const tooling::CommandArgs& args = {});
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
std::optional<scene::Entity> createBuiltinMeshEntity(scene::Scene& scene,
                                                     std::string name,
                                                     asset::AssetHandle mesh,
                                                     asset::AssetHandle material,
                                                     scene::Entity parent = {});

bool addComponent(scene::Scene& scene, scene::Entity entity, std::string_view componentType);
bool removeComponent(scene::Scene& scene, scene::Entity entity, std::string_view componentType);
bool addMaterialSlot(scene::Scene& scene,
                     scene::Entity entity,
                     asset::AssetHandle material,
                     std::optional<size_t> index = std::nullopt);
bool removeMaterialSlot(scene::Scene& scene, scene::Entity entity, size_t index);
bool addScriptBinding(scene::Scene& scene,
                      scene::Entity entity,
                      asset::AssetHandle script = {},
                      bool enabled = true,
                      std::optional<size_t> index = std::nullopt);
bool removeScriptBinding(scene::Scene& scene, scene::Entity entity, size_t index);

bool setMaterialParameters(asset::AssetHandle material, const renderer::interface::MaterialParameters& parameters);
bool saveMaterial(asset::AssetHandle material);
bool setSpriteParameters(asset::AssetHandle sprite, const asset::Sprite& parameters);
bool saveSprite(asset::AssetHandle sprite);
bool setTextureImportSettings(asset::AssetHandle texture, const renderer::interface::TextureImportSettings& settings);
bool saveTextureImportSettings(asset::AssetHandle texture);

tooling::CommandResult createProject(const std::filesystem::path& projectRoot, std::string name = {});
tooling::CommandResult openProject(const std::filesystem::path& projectPath);
tooling::CommandResult saveProject();
tooling::CommandResult createSceneFile(scene::Scene& scene, const std::filesystem::path& scenePath);
tooling::CommandResult openSceneFile(scene::Scene& scene, const std::filesystem::path& scenePath);
tooling::CommandResult saveSceneFile(scene::Scene& scene, const std::filesystem::path& scenePath);

bool beginSceneEdit(scene::Scene& scene, std::string_view commandId);
bool commitSceneEdit(scene::Scene& scene);
bool cancelSceneEdit(scene::Scene& scene);
bool hasActiveSceneEdit();

} // namespace lunalite::editor::actions
