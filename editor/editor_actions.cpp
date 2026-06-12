#include "../LunaLite/core/log.h"
#include "../LunaLite/renderer/interface/material.h"
#include "../LunaLite/renderer/interface/texture.h"
#include "../LunaLite/scene/components.h"
#include "../LunaLiteTooling/commands/asset_commands.h"
#include "../LunaLiteTooling/commands/command_manager.h"
#include "../LunaLiteTooling/commands/project_commands.h"
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

tooling::CommandResult
    executeCommand(tooling::ToolContext& context, std::string_view commandId, const tooling::CommandArgs& args)
{
    return tooling::CommandManager::get().execute(commandId, context, args);
}
} // namespace

tooling::CommandResult
    executeSceneCommand(scene::Scene& scene, std::string_view commandId, const tooling::CommandArgs& args)
{
    tooling::ToolContext context;
    context.setScene(scene);
    return executeCommand(context, commandId, args);
}

tooling::CommandResult executeAssetCommand(std::string_view commandId, const tooling::CommandArgs& args)
{
    tooling::ToolContext context;
    return executeCommand(context, commandId, args);
}

tooling::CommandResult executeAssetCommand(std::string_view commandId,
                                           const asset::AssetHandle& sourceAsset,
                                           const std::filesystem::path& targetDirectory)
{
    tooling::CommandArgs args;
    args.set("source_asset", sourceAsset);
    args.set("target_directory", targetDirectory);
    return executeAssetCommand(commandId, args);
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

std::optional<scene::Entity> createBuiltinMeshEntity(
    scene::Scene& scene, std::string name, asset::AssetHandle mesh, asset::AssetHandle material, scene::Entity parent)
{
    tooling::CommandArgs args;
    args.set("mesh", mesh);
    args.set("material", material);
    if (!name.empty()) {
        args.set("name", std::move(name));
    }
    if (parent) {
        args.set("parent_entity", entityToCommandValue(parent));
    }

    const auto result = executeSceneCommand(scene, tooling::CreateBuiltinMeshEntityCommandId, args);
    if (!result.success) {
        LUNA_CORE_ERROR("Failed to create builtin mesh entity: {}",
                        result.message.empty() ? "unknown error" : result.message);
        return std::nullopt;
    }

    return entityFromCommandResult(result);
}

bool addComponent(scene::Scene& scene, scene::Entity entity, std::string_view componentType)
{
    tooling::CommandArgs args;
    args.set("entity", entityToCommandValue(entity));
    args.set("component_type", std::string{componentType});
    return executeSceneCommand(scene, tooling::AddComponentCommandId, args).success;
}

bool removeComponent(scene::Scene& scene, scene::Entity entity, std::string_view componentType)
{
    tooling::CommandArgs args;
    args.set("entity", entityToCommandValue(entity));
    args.set("component_type", std::string{componentType});
    return executeSceneCommand(scene, tooling::RemoveComponentCommandId, args).success;
}

bool addMaterialSlot(scene::Scene& scene,
                     scene::Entity entity,
                     asset::AssetHandle material,
                     std::optional<size_t> index)
{
    tooling::CommandArgs args;
    args.set("entity", entityToCommandValue(entity));
    args.set("material", material);
    if (index) {
        args.set("index", static_cast<uint64_t>(*index));
    }
    return executeSceneCommand(scene, tooling::AddMaterialSlotCommandId, args).success;
}

bool removeMaterialSlot(scene::Scene& scene, scene::Entity entity, size_t index)
{
    tooling::CommandArgs args;
    args.set("entity", entityToCommandValue(entity));
    args.set("index", static_cast<uint64_t>(index));
    return executeSceneCommand(scene, tooling::RemoveMaterialSlotCommandId, args).success;
}

bool addScriptBinding(
    scene::Scene& scene, scene::Entity entity, asset::AssetHandle script, bool enabled, std::optional<size_t> index)
{
    tooling::CommandArgs args;
    args.set("entity", entityToCommandValue(entity));
    args.set("script", script);
    args.set("enabled", enabled);
    if (index) {
        args.set("index", static_cast<uint64_t>(*index));
    }
    return executeSceneCommand(scene, tooling::AddScriptBindingCommandId, args).success;
}

bool removeScriptBinding(scene::Scene& scene, scene::Entity entity, size_t index)
{
    tooling::CommandArgs args;
    args.set("entity", entityToCommandValue(entity));
    args.set("index", static_cast<uint64_t>(index));
    return executeSceneCommand(scene, tooling::RemoveScriptBindingCommandId, args).success;
}

bool setMaterialParameters(asset::AssetHandle material, const renderer::interface::MaterialParameters& parameters)
{
    tooling::CommandArgs args;
    args.set("material", material);
    args.set("shading_model", static_cast<uint64_t>(parameters.shading_model));
    args.set("albedo_r", static_cast<double>(parameters.albedo.r));
    args.set("albedo_g", static_cast<double>(parameters.albedo.g));
    args.set("albedo_b", static_cast<double>(parameters.albedo.b));
    args.set("albedo_a", static_cast<double>(parameters.albedo.a));
    args.set("metallic", static_cast<double>(parameters.metallic));
    args.set("roughness", static_cast<double>(parameters.roughness));
    args.set("emission_r", static_cast<double>(parameters.emission.r));
    args.set("emission_g", static_cast<double>(parameters.emission.g));
    args.set("emission_b", static_cast<double>(parameters.emission.b));
    args.set("emission_strength", static_cast<double>(parameters.emission_strength));
    args.set("normal_scale", static_cast<double>(parameters.normal_scale));
    args.set("occlusion_strength", static_cast<double>(parameters.occlusion_strength));
    args.set("albedo_texture", parameters.albedo_texture);
    args.set("normal_texture", parameters.normal_texture);
    args.set("metallic_roughness_texture", parameters.metallic_roughness_texture);
    args.set("occlusion_texture", parameters.occlusion_texture);
    args.set("emission_texture", parameters.emission_texture);
    return executeAssetCommand(tooling::SetMaterialParametersCommandId, args).success;
}

bool saveMaterial(asset::AssetHandle material)
{
    tooling::CommandArgs args;
    args.set("material", material);
    return executeAssetCommand(tooling::SaveMaterialCommandId, args).success;
}

bool setSpriteParameters(asset::AssetHandle sprite, const asset::Sprite& parameters)
{
    tooling::CommandArgs args;
    args.set("sprite", sprite);
    args.set("texture", parameters.texture);
    args.set("source_x", static_cast<uint64_t>(parameters.source_rect.x));
    args.set("source_y", static_cast<uint64_t>(parameters.source_rect.y));
    args.set("source_width", static_cast<uint64_t>(parameters.source_rect.width));
    args.set("source_height", static_cast<uint64_t>(parameters.source_rect.height));
    args.set("pivot_x", static_cast<double>(parameters.pivot.x));
    args.set("pivot_y", static_cast<double>(parameters.pivot.y));
    args.set("pixels_per_unit", static_cast<double>(parameters.pixels_per_unit));
    args.set("flip_y", parameters.flip_y);
    args.set("color_space", static_cast<uint64_t>(parameters.color_space));
    return executeAssetCommand(tooling::SetSpriteParametersCommandId, args).success;
}

bool saveSprite(asset::AssetHandle sprite)
{
    tooling::CommandArgs args;
    args.set("sprite", sprite);
    return executeAssetCommand(tooling::SaveSpriteCommandId, args).success;
}

bool setTextureImportSettings(asset::AssetHandle texture, const renderer::interface::TextureImportSettings& settings)
{
    tooling::CommandArgs args;
    args.set("texture", texture);
    args.set("generate_mipmaps", settings.generate_mipmaps);
    args.set("color_space", static_cast<uint64_t>(settings.color_space));
    args.set("min_filter", static_cast<uint64_t>(settings.sampler.min_filter));
    args.set("mag_filter", static_cast<uint64_t>(settings.sampler.mag_filter));
    args.set("mip_filter", static_cast<uint64_t>(settings.sampler.mip_filter));
    args.set("address_u", static_cast<uint64_t>(settings.sampler.address_u));
    args.set("address_v", static_cast<uint64_t>(settings.sampler.address_v));
    args.set("address_w", static_cast<uint64_t>(settings.sampler.address_w));
    return executeAssetCommand(tooling::SetTextureImportSettingsCommandId, args).success;
}

bool saveTextureImportSettings(asset::AssetHandle texture)
{
    tooling::CommandArgs args;
    args.set("texture", texture);
    return executeAssetCommand(tooling::SaveTextureImportSettingsCommandId, args).success;
}

tooling::CommandResult createProject(const std::filesystem::path& projectRoot, std::string name)
{
    tooling::CommandArgs args;
    args.set("project_root", projectRoot);
    if (!name.empty()) {
        args.set("name", std::move(name));
    }
    return executeAssetCommand(tooling::CreateProjectCommandId, args);
}

tooling::CommandResult openProject(const std::filesystem::path& projectPath)
{
    tooling::CommandArgs args;
    args.set("project_path", projectPath);
    return executeAssetCommand(tooling::OpenProjectCommandId, args);
}

tooling::CommandResult saveProject()
{
    return executeAssetCommand(tooling::SaveProjectCommandId);
}

tooling::CommandResult createSceneFile(scene::Scene& scene, const std::filesystem::path& scenePath)
{
    tooling::CommandArgs args;
    args.set("scene_path", scenePath);
    return executeSceneCommand(scene, tooling::CreateSceneFileCommandId, args);
}

tooling::CommandResult openSceneFile(scene::Scene& scene, const std::filesystem::path& scenePath)
{
    tooling::CommandArgs args;
    args.set("scene_path", scenePath);
    return executeSceneCommand(scene, tooling::OpenSceneFileCommandId, args);
}

tooling::CommandResult saveSceneFile(scene::Scene& scene, const std::filesystem::path& scenePath)
{
    tooling::CommandArgs args;
    args.set("scene_path", scenePath);
    return executeSceneCommand(scene, tooling::SaveSceneFileCommandId, args);
}

bool beginSceneEdit(scene::Scene& scene, std::string_view commandId)
{
    tooling::ToolContext context;
    context.setScene(scene);
    return tooling::CommandManager::get().beginSceneEdit(commandId, context);
}

bool commitSceneEdit(scene::Scene& scene)
{
    tooling::ToolContext context;
    context.setScene(scene);
    return tooling::CommandManager::get().commitSceneEdit(context);
}

bool cancelSceneEdit(scene::Scene& scene)
{
    tooling::ToolContext context;
    context.setScene(scene);
    return tooling::CommandManager::get().cancelSceneEdit(context);
}

bool hasActiveSceneEdit()
{
    return tooling::CommandManager::get().hasActiveSceneEdit();
}

} // namespace lunalite::editor::actions
