#include "scene_commands.h"

#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/scene/components.h"
#include "../../LunaLite/scene/scene.h"
#include "command_registry.h"

#include <cstdint>

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace lunalite::tooling {
namespace {
using EntityUnderlying = std::underlying_type_t<entt::entity>;

uint64_t entityToValue(scene::Entity entity)
{
    return static_cast<uint64_t>(static_cast<EntityUnderlying>(entity.getHandle()));
}

scene::Entity entityFromValue(uint64_t value)
{
    return scene::Entity{static_cast<entt::entity>(static_cast<EntityUnderlying>(value))};
}

std::optional<scene::Entity> entityArg(const CommandArgs& args, std::string_view key)
{
    const auto value = args.get<uint64_t>(key);
    if (!value) {
        return std::nullopt;
    }
    return entityFromValue(*value);
}

scene::Scene* activeScene(ToolContext& context)
{
    return context.scene();
}

bool boolArg(const CommandArgs& args, std::string_view key, bool fallback)
{
    const auto value = args.get<bool>(key);
    return value ? *value : fallback;
}

std::optional<asset::AssetType> assetTypeArg(const CommandArgs& args)
{
    const auto value = args.get<uint64_t>("asset_type");
    if (!value) {
        return std::nullopt;
    }

    switch (*value) {
        case static_cast<uint64_t>(asset::AssetType::Mesh):
            return asset::AssetType::Mesh;
        case static_cast<uint64_t>(asset::AssetType::Material):
            return asset::AssetType::Material;
        case static_cast<uint64_t>(asset::AssetType::Prefab):
            return asset::AssetType::Prefab;
        case static_cast<uint64_t>(asset::AssetType::Script):
            return asset::AssetType::Script;
        case static_cast<uint64_t>(asset::AssetType::Texture):
            return asset::AssetType::Texture;
        case static_cast<uint64_t>(asset::AssetType::Sprite):
            return asset::AssetType::Sprite;
        case static_cast<uint64_t>(asset::AssetType::None):
        default:
            return asset::AssetType::None;
    }
}

asset::AssetType resolveAssetType(ToolContext& context, asset::AssetHandle handle, const CommandArgs& args)
{
    if (const auto* metadata = context.assetManager().getMetadata(handle)) {
        return metadata->Type;
    }

    return assetTypeArg(args).value_or(asset::AssetType::None);
}

asset::AssetHandle resolvePrefabCompanion(asset::AssetManager& assetManager, const asset::AssetMetadata* metadata)
{
    if (metadata == nullptr || metadata->Type != asset::AssetType::Mesh) {
        return {};
    }

    const auto prefabPath = metadata->FilePath.parent_path() / (metadata->FilePath.stem().string() + ".lunaprefab");
    return assetManager.getHandleByRelativePath(prefabPath);
}

void setEntityTagFromAsset(scene::Scene& scene, scene::Entity entity, const asset::AssetMetadata* metadata)
{
    if (metadata == nullptr || !scene.hasComponent<scene::TagComponent>(entity)) {
        return;
    }

    auto& tag = scene.getComponent<scene::TagComponent>(entity);
    tag.tag = metadata->Name.empty() ? metadata->FilePath.stem().string() : metadata->Name;
}

void addScriptToEntity(scene::Scene& scene, scene::Entity entity, asset::AssetHandle handle)
{
    if (!scene.hasComponent<scene::ScriptComponent>(entity)) {
        scene.addComponent<scene::ScriptComponent>(entity);
    }

    auto& script = scene.getComponent<scene::ScriptComponent>(entity);
    script.scripts.push_back({handle, true});
}

scene::Entity createMeshRendererEntity(scene::Scene& scene,
                                       asset::AssetHandle meshHandle,
                                       const asset::AssetMetadata* metadata,
                                       scene::Entity parent = {})
{
    auto entity = scene.createEntity();
    auto& meshRenderer = scene.addComponent<scene::MeshRendererComponent>(entity);
    meshRenderer.mesh = meshHandle;
    if (parent && !scene.setParent(entity, parent, false)) {
        scene.destroyEntity(entity);
        return {};
    }
    setEntityTagFromAsset(scene, entity, metadata);
    return entity;
}

scene::Entity createSpriteRendererEntity(scene::Scene& scene,
                                         asset::AssetHandle spriteHandle,
                                         const asset::AssetMetadata* metadata,
                                         scene::Entity parent = {})
{
    auto entity = scene.createEntity();
    auto& spriteRenderer = scene.addComponent<scene::SpriteRendererComponent>(entity);
    spriteRenderer.sprite = spriteHandle;
    if (parent && !scene.setParent(entity, parent, false)) {
        scene.destroyEntity(entity);
        return {};
    }
    setEntityTagFromAsset(scene, entity, metadata);
    return entity;
}

CommandResult entityResult(std::string message, scene::Entity entity, bool created)
{
    auto result = CommandResult::ok(std::move(message));
    result.set("entity", entityToValue(entity));
    if (created) {
        result.set("created_entity", entityToValue(entity));
    } else {
        result.set("affected_entity", entityToValue(entity));
    }
    return result;
}

CommandResult createEntity(ToolContext& context, const CommandArgs& args)
{
    auto* scene = activeScene(context);
    if (scene == nullptr) {
        return CommandResult::fail("scene.create_entity requires an active scene");
    }

    const auto entity = scene->createEntity();

    if (const auto name = args.get<std::string>("name")) {
        if (scene->hasComponent<scene::TagComponent>(entity)) {
            scene->getComponent<scene::TagComponent>(entity).tag = *name;
        }
    }

    if (const auto parent = entityArg(args, "parent_entity")) {
        if (!scene->setParent(entity, *parent, false)) {
            scene->destroyEntity(entity);
            return CommandResult::fail("scene.create_entity failed to assign parent_entity");
        }
    }

    auto result = CommandResult::ok("Entity created");
    result.set("created_entity", entityToValue(entity));
    return result;
}

CommandResult deleteEntity(ToolContext& context, const CommandArgs& args)
{
    auto* scene = activeScene(context);
    if (scene == nullptr) {
        return CommandResult::fail("scene.delete_entity requires an active scene");
    }

    const auto entity = entityArg(args, "entity");
    if (!entity) {
        return CommandResult::fail("scene.delete_entity requires entity");
    }
    if (!scene->isValidEntity(*entity)) {
        return CommandResult::fail("scene.delete_entity requires a valid entity");
    }

    const auto deletedEntity = entityToValue(*entity);
    scene->destroyEntity(*entity);

    auto result = CommandResult::ok("Entity deleted");
    result.set("deleted_entity", deletedEntity);
    return result;
}

CommandResult setParent(ToolContext& context, const CommandArgs& args)
{
    auto* scene = activeScene(context);
    if (scene == nullptr) {
        return CommandResult::fail("scene.set_parent requires an active scene");
    }

    const auto entity = entityArg(args, "entity");
    if (!entity) {
        return CommandResult::fail("scene.set_parent requires entity");
    }

    const auto parent = entityArg(args, "parent_entity");
    if (!parent) {
        return CommandResult::fail("scene.set_parent requires parent_entity");
    }

    const bool keepWorldTransform = boolArg(args, "keep_world_transform", true);
    if (!scene->setParent(*entity, *parent, keepWorldTransform)) {
        return CommandResult::fail("scene.set_parent failed");
    }

    auto result = CommandResult::ok("Entity parent set");
    result.set("entity", entityToValue(*entity));
    result.set("parent_entity", entityToValue(*parent));
    return result;
}

CommandResult clearParent(ToolContext& context, const CommandArgs& args)
{
    auto* scene = activeScene(context);
    if (scene == nullptr) {
        return CommandResult::fail("scene.clear_parent requires an active scene");
    }

    const auto entity = entityArg(args, "entity");
    if (!entity) {
        return CommandResult::fail("scene.clear_parent requires entity");
    }

    const bool keepWorldTransform = boolArg(args, "keep_world_transform", true);
    if (!scene->clearParent(*entity, keepWorldTransform)) {
        return CommandResult::fail("scene.clear_parent failed");
    }

    auto result = CommandResult::ok("Entity parent cleared");
    result.set("entity", entityToValue(*entity));
    return result;
}

CommandResult createEntityFromAsset(ToolContext& context, const CommandArgs& args)
{
    auto* scene = activeScene(context);
    if (scene == nullptr) {
        return CommandResult::fail("scene.create_entity_from_asset requires an active scene");
    }

    const auto sourceAsset = args.get<asset::AssetHandle>("source_asset");
    if (!sourceAsset || !sourceAsset->isValid()) {
        return CommandResult::fail("scene.create_entity_from_asset requires a valid source_asset");
    }

    const auto targetEntity = entityArg(args, "target_entity");
    if (targetEntity && !scene->isValidEntity(*targetEntity)) {
        return CommandResult::fail("scene.create_entity_from_asset target_entity is invalid");
    }
    const auto parent = targetEntity.value_or(scene::Entity{});

    auto& assetManager = context.assetManager();
    const auto* metadata = assetManager.getMetadata(*sourceAsset);
    const auto type = resolveAssetType(context, *sourceAsset, args);

    if (type == asset::AssetType::Prefab) {
        const auto root = scene->instantiatePrefab(*sourceAsset, parent);
        if (!root) {
            return CommandResult::fail("scene.create_entity_from_asset failed to instantiate prefab");
        }
        return entityResult("Prefab instantiated", root, true);
    }

    if (type == asset::AssetType::Mesh) {
        const auto prefabHandle = resolvePrefabCompanion(assetManager, metadata);
        if (prefabHandle.isValid()) {
            const auto root = scene->instantiatePrefab(prefabHandle, parent);
            if (root) {
                return entityResult("Mesh companion prefab instantiated", root, true);
            }
        }

        const auto entity = createMeshRendererEntity(*scene, *sourceAsset, metadata, parent);
        if (!entity) {
            return CommandResult::fail("scene.create_entity_from_asset failed to create mesh renderer entity");
        }
        return entityResult("Mesh renderer entity created", entity, true);
    }

    if (type == asset::AssetType::Sprite) {
        const auto entity = createSpriteRendererEntity(*scene, *sourceAsset, metadata, parent);
        if (!entity) {
            return CommandResult::fail("scene.create_entity_from_asset failed to create sprite renderer entity");
        }
        return entityResult("Sprite renderer entity created", entity, true);
    }

    if (type == asset::AssetType::Script) {
        const bool useTargetEntity = targetEntity.has_value();
        const auto entity = useTargetEntity ? *targetEntity : scene->createEntity();
        addScriptToEntity(*scene, entity, *sourceAsset);
        if (!useTargetEntity) {
            setEntityTagFromAsset(*scene, entity, metadata);
        }
        return entityResult(
            useTargetEntity ? "Script added to entity" : "Script entity created", entity, !useTargetEntity);
    }

    return CommandResult::fail("scene.create_entity_from_asset does not support this asset type");
}
} // namespace

void registerSceneCommands(CommandRegistry& registry)
{
    registry.registerCommand(CommandDesc{
        .id = std::string{CreateEntityCommandId},
        .label = "Create Entity",
        .category = "Scene",
        .execute = createEntity,
    });

    registry.registerCommand(CommandDesc{
        .id = std::string{DeleteEntityCommandId},
        .label = "Delete Entity",
        .category = "Scene",
        .execute = deleteEntity,
    });

    registry.registerCommand(CommandDesc{
        .id = std::string{SetParentCommandId},
        .label = "Set Parent",
        .category = "Scene",
        .execute = setParent,
    });

    registry.registerCommand(CommandDesc{
        .id = std::string{ClearParentCommandId},
        .label = "Clear Parent",
        .category = "Scene",
        .execute = clearParent,
    });

    registry.registerCommand(CommandDesc{
        .id = std::string{CreateEntityFromAssetCommandId},
        .label = "Create Entity From Asset",
        .category = "Scene",
        .execute = createEntityFromAsset,
    });
}

} // namespace lunalite::tooling
