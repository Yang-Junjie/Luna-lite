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

tooling::CommandResult executeSceneCommand(scene::Scene& scene,
                                           std::string_view commandId,
                                           const tooling::CommandArgs& args = {});
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

} // namespace lunalite::editor::actions
