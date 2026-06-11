#include "../LunaLite/core/log.h"
#include "../LunaLite/scene/components.h"
#include "../LunaLiteTooling/commands/command_manager.h"
#include "../LunaLiteTooling/commands/scene_commands.h"
#include "../LunaLiteTooling/context/tool_context.h"
#include "editor_actions.h"

#include <cstdint>

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace lunalite::editor::actions {
namespace {
using EntityUnderlying = std::underlying_type_t<entt::entity>;

uint64_t entityToCommandValue(scene::Entity entity)
{
    return static_cast<uint64_t>(static_cast<EntityUnderlying>(entity.getHandle()));
}

scene::Entity entityFromCommandValue(uint64_t value)
{
    return scene::Entity{static_cast<entt::entity>(static_cast<EntityUnderlying>(value))};
}

tooling::CommandResult executeCommand(tooling::ToolContext& context,
                                      std::string_view commandId,
                                      const tooling::CommandArgs& args)
{
    return tooling::CommandManager::get().execute(commandId, context, args);
}
} // namespace

tooling::CommandResult executeSceneCommand(scene::Scene& scene,
                                           std::string_view commandId,
                                           const tooling::CommandArgs& args)
{
    tooling::ToolContext context;
    context.setScene(scene);
    return executeCommand(context, commandId, args);
}

tooling::CommandResult executeAssetCommand(std::string_view commandId,
                                           const asset::AssetHandle& sourceAsset,
                                           const std::filesystem::path& targetDirectory)
{
    tooling::ToolContext context;
    tooling::CommandArgs args;
    args.set("source_asset", sourceAsset);
    args.set("target_directory", targetDirectory);
    return executeCommand(context, commandId, args);
}

std::optional<scene::Entity> entityFromCommandResult(const tooling::CommandResult& result)
{
    if (const auto* created = result.get<uint64_t>("created_entity")) {
        return entityFromCommandValue(*created);
    }
    if (const auto* affected = result.get<uint64_t>("affected_entity")) {
        return entityFromCommandValue(*affected);
    }
    if (const auto* entity = result.get<uint64_t>("entity")) {
        return entityFromCommandValue(*entity);
    }
    return std::nullopt;
}

std::optional<scene::Entity> createEntity(scene::Scene& scene, std::string name, scene::Entity parent)
{
    tooling::CommandArgs args;
    if (!name.empty()) {
        args.set("name", std::move(name));
    }
    if (parent) {
        args.set("parent_entity", entityToCommandValue(parent));
    }

    const auto result = executeSceneCommand(scene, tooling::CreateEntityCommandId, args);
    if (!result.success) {
        return std::nullopt;
    }
    return entityFromCommandResult(result);
}

bool deleteEntity(scene::Scene& scene, scene::Entity entity)
{
    tooling::CommandArgs args;
    args.set("entity", entityToCommandValue(entity));
    return executeSceneCommand(scene, tooling::DeleteEntityCommandId, args).success;
}

bool setParent(scene::Scene& scene, scene::Entity entity, scene::Entity parent, bool keepWorldTransform)
{
    tooling::CommandArgs args;
    args.set("entity", entityToCommandValue(entity));
    args.set("parent_entity", entityToCommandValue(parent));
    args.set("keep_world_transform", keepWorldTransform);
    return executeSceneCommand(scene, tooling::SetParentCommandId, args).success;
}

bool clearParent(scene::Scene& scene, scene::Entity entity, bool keepWorldTransform)
{
    tooling::CommandArgs args;
    args.set("entity", entityToCommandValue(entity));
    args.set("keep_world_transform", keepWorldTransform);
    return executeSceneCommand(scene, tooling::ClearParentCommandId, args).success;
}

std::optional<scene::Entity> createEntityFromAsset(scene::Scene& scene,
                                                   asset::AssetHandle sourceAsset,
                                                   asset::AssetType type,
                                                   scene::Entity targetEntity)
{
    if (!sourceAsset.isValid()) {
        return std::nullopt;
    }

    tooling::CommandArgs args;
    args.set("source_asset", sourceAsset);
    args.set("asset_type", static_cast<uint64_t>(type));
    if (targetEntity) {
        args.set("target_entity", entityToCommandValue(targetEntity));
    }

    const auto result = executeSceneCommand(scene, tooling::CreateEntityFromAssetCommandId, args);
    if (!result.success) {
        LUNA_CORE_ERROR("Failed to create entity from asset '{}': {}",
                        sourceAsset.toString(),
                        result.message.empty() ? "unknown error" : result.message);
        return std::nullopt;
    }

    return entityFromCommandResult(result);
}

} // namespace lunalite::editor::actions
