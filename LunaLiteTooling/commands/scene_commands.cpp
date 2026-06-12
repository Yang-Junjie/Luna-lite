#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/asset/builtin/builtin_assets.h"
#include "../../LunaLite/scene/components.h"
#include "../../LunaLite/scene/scene.h"
#include "command_registry.h"
#include "scene_commands.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <memory>
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

asset::AssetHandle assetArg(const CommandArgs& args, std::string_view key, asset::AssetHandle fallback = {})
{
    const auto value = args.get<asset::AssetHandle>(key);
    return value ? *value : fallback;
}

std::optional<size_t> indexArg(const CommandArgs& args, std::string_view key)
{
    const auto value = args.get<uint64_t>(key);
    if (!value) {
        return std::nullopt;
    }
    return static_cast<size_t>(*value);
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

std::optional<float> floatArg(const CommandArgs& args, std::string_view key)
{
    if (const auto value = args.get<double>(key)) {
        return static_cast<float>(*value);
    }
    if (const auto value = args.get<int64_t>(key)) {
        return static_cast<float>(*value);
    }
    if (const auto value = args.get<uint64_t>(key)) {
        return static_cast<float>(*value);
    }
    return std::nullopt;
}

std::optional<int32_t> int32Arg(const CommandArgs& args, std::string_view key)
{
    if (const auto value = args.get<int64_t>(key)) {
        return static_cast<int32_t>(
            std::clamp<int64_t>(*value, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()));
    }
    if (const auto value = args.get<uint64_t>(key)) {
        return static_cast<int32_t>(std::min<uint64_t>(*value, std::numeric_limits<int32_t>::max()));
    }
    return std::nullopt;
}

std::optional<uint32_t> uint32Arg(const CommandArgs& args, std::string_view key)
{
    if (const auto value = args.get<uint64_t>(key)) {
        return static_cast<uint32_t>(std::min<uint64_t>(*value, std::numeric_limits<uint32_t>::max()));
    }
    if (const auto value = args.get<int64_t>(key)) {
        return static_cast<uint32_t>(
            std::clamp<int64_t>(*value, 0, static_cast<int64_t>(std::numeric_limits<uint32_t>::max())));
    }
    return std::nullopt;
}

std::optional<uint64_t> uint64Arg(const CommandArgs& args, std::string_view key)
{
    if (const auto value = args.get<uint64_t>(key)) {
        return *value;
    }
    if (const auto value = args.get<int64_t>(key)) {
        return static_cast<uint64_t>(std::max<int64_t>(*value, 0));
    }
    return std::nullopt;
}

float quatLengthSquared(const glm::quat& rotation)
{
    return rotation.w * rotation.w + rotation.x * rotation.x + rotation.y * rotation.y + rotation.z * rotation.z;
}

glm::quat safeNormalize(glm::quat rotation)
{
    if (quatLengthSquared(rotation) <= 0.000001f) {
        return glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
    }

    return glm::normalize(rotation);
}

template <typename Component>
CommandResult
    componentEditResult(std::string_view commandId, scene::Scene& scene, scene::Entity entity, Component*& component)
{
    if (!scene.isValidEntity(entity)) {
        return CommandResult::fail(std::string{commandId} + " requires a valid entity");
    }
    if (!scene.hasComponent<Component>(entity)) {
        return CommandResult::fail(std::string{commandId} + " requires component");
    }

    component = &scene.getComponent<Component>(entity);
    return CommandResult::ok();
}

template <typename Component>
CommandResult resolveEditableComponent(ToolContext& context,
                                       const CommandArgs& args,
                                       std::string_view commandId,
                                       scene::Entity& entity,
                                       Component*& component)
{
    auto* scene = activeScene(context);
    if (scene == nullptr) {
        return CommandResult::fail(std::string{commandId} + " requires an active scene");
    }

    const auto entityArgValue = entityArg(args, "entity");
    if (!entityArgValue) {
        return CommandResult::fail(std::string{commandId} + " requires entity");
    }

    entity = *entityArgValue;
    return componentEditResult(commandId, *scene, entity, component);
}

CommandResult editResult(std::string message, scene::Entity entity)
{
    auto result = CommandResult::ok(std::move(message));
    result.set("entity", entityToValue(entity));
    result.set("affected_entity", entityToValue(entity));
    return result;
}

void applyTransformArgs(scene::TransformComponent& transform, const CommandArgs& args)
{
    if (const auto value = floatArg(args, "translation_x")) {
        transform.translation.x = *value;
    }
    if (const auto value = floatArg(args, "translation_y")) {
        transform.translation.y = *value;
    }
    if (const auto value = floatArg(args, "translation_z")) {
        transform.translation.z = *value;
    }

    const auto hasQuaternionArg = args.contains("rotation_w") || args.contains("rotation_x") ||
                                  args.contains("rotation_y") || args.contains("rotation_z");
    if (hasQuaternionArg) {
        auto rotation = transform.rotation;
        if (const auto value = floatArg(args, "rotation_w")) {
            rotation.w = *value;
        }
        if (const auto value = floatArg(args, "rotation_x")) {
            rotation.x = *value;
        }
        if (const auto value = floatArg(args, "rotation_y")) {
            rotation.y = *value;
        }
        if (const auto value = floatArg(args, "rotation_z")) {
            rotation.z = *value;
        }
        transform.rotation = safeNormalize(rotation);
    }

    if (args.contains("rotation_degrees_x") || args.contains("rotation_degrees_y") ||
        args.contains("rotation_degrees_z")) {
        glm::vec3 rotationDegrees = glm::degrees(glm::eulerAngles(safeNormalize(transform.rotation)));
        if (const auto value = floatArg(args, "rotation_degrees_x")) {
            rotationDegrees.x = *value;
        }
        if (const auto value = floatArg(args, "rotation_degrees_y")) {
            rotationDegrees.y = *value;
        }
        if (const auto value = floatArg(args, "rotation_degrees_z")) {
            rotationDegrees.z = *value;
        }
        transform.rotation = safeNormalize(glm::quat{glm::radians(rotationDegrees)});
    }

    if (const auto value = floatArg(args, "scale_x")) {
        transform.scale.x = *value;
    }
    if (const auto value = floatArg(args, "scale_y")) {
        transform.scale.y = *value;
    }
    if (const auto value = floatArg(args, "scale_z")) {
        transform.scale.z = *value;
    }
}

void applyMeshRendererArgs(scene::MeshRendererComponent& meshRenderer, const CommandArgs& args)
{
    meshRenderer.mesh = assetArg(args, "mesh", meshRenderer.mesh);
    if (const auto value = args.get<bool>("cast_shadow")) {
        meshRenderer.cast_shadow = *value;
    }
    if (const auto value = uint32Arg(args, "submesh_start")) {
        meshRenderer.submesh_start = *value;
    }
    if (const auto value = uint32Arg(args, "submesh_count")) {
        meshRenderer.submesh_count = *value;
    }

    if (const auto index = indexArg(args, "material_index")) {
        if (*index < meshRenderer.materials.size()) {
            meshRenderer.materials[*index] = assetArg(args, "material", meshRenderer.materials[*index]);
        }
    }
}

void applySpriteRendererArgs(scene::SpriteRendererComponent& spriteRenderer, const CommandArgs& args)
{
    spriteRenderer.sprite = assetArg(args, "sprite", spriteRenderer.sprite);
    if (const auto value = floatArg(args, "color_r")) {
        spriteRenderer.color.r = *value;
    }
    if (const auto value = floatArg(args, "color_g")) {
        spriteRenderer.color.g = *value;
    }
    if (const auto value = floatArg(args, "color_b")) {
        spriteRenderer.color.b = *value;
    }
    if (const auto value = floatArg(args, "color_a")) {
        spriteRenderer.color.a = *value;
    }
    if (const auto value = int32Arg(args, "sorting_layer")) {
        spriteRenderer.sorting_layer = *value;
    }
    if (const auto value = int32Arg(args, "order_in_layer")) {
        spriteRenderer.order_in_layer = *value;
    }
    if (const auto value = args.get<bool>("depth_test")) {
        spriteRenderer.depth_test = *value;
    }
}

void applyCameraArgs(scene::CameraComponent& camera, const CommandArgs& args)
{
    if (const auto value = args.get<bool>("primary")) {
        camera.primary = *value;
    }
    if (const auto value = floatArg(args, "exposure")) {
        camera.camera.setExposure(*value);
    }
    if (const auto value = uint64Arg(args, "projection_type")) {
        using ProjectionType = renderer::interface::Camera::ProjectionType;
        if (*value == static_cast<uint64_t>(ProjectionType::Orthographic) || *value == 1u) {
            camera.camera.setOrthographic(10.0f, 0.1f, 100.0f);
        } else {
            camera.camera.setPerspective(glm::radians(45.0f), 0.1f, 100.0f);
        }
    }
}

void applyDirectionalLightArgs(scene::DirectionalLightComponent& light, const CommandArgs& args)
{
    if (const auto value = floatArg(args, "color_r")) {
        light.color.r = *value;
    }
    if (const auto value = floatArg(args, "color_g")) {
        light.color.g = *value;
    }
    if (const auto value = floatArg(args, "color_b")) {
        light.color.b = *value;
    }
    if (const auto value = floatArg(args, "intensity")) {
        light.intensity = std::max(*value, 0.0f);
    }
    if (const auto value = args.get<bool>("shadow_enabled")) {
        light.shadow.enabled = *value;
    }
    if (const auto value = uint32Arg(args, "shadow_map_size")) {
        light.shadow.map_size = std::max(*value, 1u);
    }
    if (const auto value = floatArg(args, "shadow_max_distance")) {
        light.shadow.max_distance = std::max(*value, 0.0f);
    }
    if (const auto value = floatArg(args, "shadow_bias")) {
        light.shadow.bias = std::max(*value, 0.0f);
    }
    if (const auto value = floatArg(args, "shadow_normal_bias")) {
        light.shadow.normal_bias = std::max(*value, 0.0f);
    }
    if (const auto value = uint32Arg(args, "shadow_pcf_radius")) {
        light.shadow.pcf_radius = std::clamp(*value, 0u, 4u);
    }
    if (const auto value = uint32Arg(args, "shadow_cascade_count")) {
        light.shadow.cascade_count = std::clamp(*value, 1u, 4u);
    }
    if (const auto value = floatArg(args, "shadow_cascade_split_lambda")) {
        light.shadow.cascade_split_lambda = std::clamp(*value, 0.0f, 1.0f);
    }
    if (const auto value = floatArg(args, "shadow_cascade_seam_blend")) {
        light.shadow.cascade_seam_blend = std::max(*value, 0.0f);
    }
    if (const auto value = floatArg(args, "shadow_cascade_caster_depth_padding")) {
        light.shadow.cascade_caster_depth_padding = std::max(*value, 0.0f);
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

CommandResult createBuiltinMeshEntity(ToolContext& context, const CommandArgs& args)
{
    auto* scene = activeScene(context);
    if (scene == nullptr) {
        return CommandResult::fail("scene.create_builtin_mesh_entity requires an active scene");
    }

    const auto mesh = args.get<asset::AssetHandle>("mesh");
    if (!mesh || !mesh->isValid()) {
        return CommandResult::fail("scene.create_builtin_mesh_entity requires a valid mesh");
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
            return CommandResult::fail("scene.create_builtin_mesh_entity failed to assign parent_entity");
        }
    }

    auto& meshRenderer = scene->addComponent<scene::MeshRendererComponent>(entity);
    meshRenderer.mesh = *mesh;
    meshRenderer.materials.push_back(assetArg(args, "material", asset::builtin::defaultMaterialHandle()));

    return entityResult("Builtin mesh entity created", entity, true);
}

CommandResult addComponent(ToolContext& context, const CommandArgs& args)
{
    auto* scene = activeScene(context);
    if (scene == nullptr) {
        return CommandResult::fail("scene.add_component requires an active scene");
    }

    const auto entity = entityArg(args, "entity");
    if (!entity || !scene->isValidEntity(*entity)) {
        return CommandResult::fail("scene.add_component requires a valid entity");
    }

    const auto componentType = args.get<std::string>("component_type");
    if (!componentType || componentType->empty()) {
        return CommandResult::fail("scene.add_component requires component_type");
    }

    const std::string_view componentTypeView{*componentType};

    if (componentTypeView == MeshRendererComponentType) {
        if (scene->hasComponent<scene::MeshRendererComponent>(*entity)) {
            return CommandResult::fail("Entity already has MeshRenderer component");
        }
        scene->addComponent<scene::MeshRendererComponent>(*entity);
    } else if (componentTypeView == SpriteRendererComponentType) {
        if (scene->hasComponent<scene::SpriteRendererComponent>(*entity)) {
            return CommandResult::fail("Entity already has SpriteRenderer component");
        }
        scene->addComponent<scene::SpriteRendererComponent>(*entity);
    } else if (componentTypeView == ScriptComponentType) {
        if (scene->hasComponent<scene::ScriptComponent>(*entity)) {
            return CommandResult::fail("Entity already has Script component");
        }
        scene->addComponent<scene::ScriptComponent>(*entity);
    } else if (componentTypeView == CameraComponentType) {
        if (scene->hasComponent<scene::CameraComponent>(*entity)) {
            return CommandResult::fail("Entity already has Camera component");
        }
        scene->addComponent<scene::CameraComponent>(*entity);
    } else if (componentTypeView == DirectionalLightComponentType) {
        if (scene->hasComponent<scene::DirectionalLightComponent>(*entity)) {
            return CommandResult::fail("Entity already has DirectionalLight component");
        }
        scene->addComponent<scene::DirectionalLightComponent>(*entity);
    } else {
        return CommandResult::fail("scene.add_component does not support this component_type");
    }

    auto result = CommandResult::ok("Component added");
    result.set("entity", entityToValue(*entity));
    result.set("component_type", *componentType);
    return result;
}

CommandResult removeComponent(ToolContext& context, const CommandArgs& args)
{
    auto* scene = activeScene(context);
    if (scene == nullptr) {
        return CommandResult::fail("scene.remove_component requires an active scene");
    }

    const auto entity = entityArg(args, "entity");
    if (!entity || !scene->isValidEntity(*entity)) {
        return CommandResult::fail("scene.remove_component requires a valid entity");
    }

    const auto componentType = args.get<std::string>("component_type");
    if (!componentType || componentType->empty()) {
        return CommandResult::fail("scene.remove_component requires component_type");
    }

    const std::string_view componentTypeView{*componentType};

    if (componentTypeView == MeshRendererComponentType) {
        if (!scene->hasComponent<scene::MeshRendererComponent>(*entity)) {
            return CommandResult::fail("Entity does not have MeshRenderer component");
        }
        scene->removeComponent<scene::MeshRendererComponent>(*entity);
    } else if (componentTypeView == SpriteRendererComponentType) {
        if (!scene->hasComponent<scene::SpriteRendererComponent>(*entity)) {
            return CommandResult::fail("Entity does not have SpriteRenderer component");
        }
        scene->removeComponent<scene::SpriteRendererComponent>(*entity);
    } else if (componentTypeView == ScriptComponentType) {
        if (!scene->hasComponent<scene::ScriptComponent>(*entity)) {
            return CommandResult::fail("Entity does not have Script component");
        }
        scene->removeComponent<scene::ScriptComponent>(*entity);
    } else if (componentTypeView == CameraComponentType) {
        if (!scene->hasComponent<scene::CameraComponent>(*entity)) {
            return CommandResult::fail("Entity does not have Camera component");
        }
        scene->removeComponent<scene::CameraComponent>(*entity);
    } else if (componentTypeView == DirectionalLightComponentType) {
        if (!scene->hasComponent<scene::DirectionalLightComponent>(*entity)) {
            return CommandResult::fail("Entity does not have DirectionalLight component");
        }
        scene->removeComponent<scene::DirectionalLightComponent>(*entity);
    } else {
        return CommandResult::fail("scene.remove_component does not support this component_type");
    }

    auto result = CommandResult::ok("Component removed");
    result.set("entity", entityToValue(*entity));
    result.set("component_type", *componentType);
    return result;
}

CommandResult addMaterialSlot(ToolContext& context, const CommandArgs& args)
{
    auto* scene = activeScene(context);
    if (scene == nullptr) {
        return CommandResult::fail("scene.add_material_slot requires an active scene");
    }

    const auto entity = entityArg(args, "entity");
    if (!entity || !scene->isValidEntity(*entity)) {
        return CommandResult::fail("scene.add_material_slot requires a valid entity");
    }
    if (!scene->hasComponent<scene::MeshRendererComponent>(*entity)) {
        return CommandResult::fail("scene.add_material_slot requires MeshRenderer component");
    }

    auto& meshRenderer = scene->getComponent<scene::MeshRendererComponent>(*entity);
    const auto material = assetArg(args, "material", asset::builtin::defaultMaterialHandle());
    const auto index = indexArg(args, "index").value_or(meshRenderer.materials.size());
    if (index > meshRenderer.materials.size()) {
        return CommandResult::fail("scene.add_material_slot index is out of range");
    }

    meshRenderer.materials.insert(meshRenderer.materials.begin() + static_cast<std::ptrdiff_t>(index), material);

    auto result = CommandResult::ok("Material slot added");
    result.set("entity", entityToValue(*entity));
    result.set("index", static_cast<uint64_t>(index));
    result.set("material", material);
    return result;
}

CommandResult removeMaterialSlot(ToolContext& context, const CommandArgs& args)
{
    auto* scene = activeScene(context);
    if (scene == nullptr) {
        return CommandResult::fail("scene.remove_material_slot requires an active scene");
    }

    const auto entity = entityArg(args, "entity");
    if (!entity || !scene->isValidEntity(*entity)) {
        return CommandResult::fail("scene.remove_material_slot requires a valid entity");
    }
    if (!scene->hasComponent<scene::MeshRendererComponent>(*entity)) {
        return CommandResult::fail("scene.remove_material_slot requires MeshRenderer component");
    }

    auto& meshRenderer = scene->getComponent<scene::MeshRendererComponent>(*entity);
    const auto index = indexArg(args, "index");
    if (!index || *index >= meshRenderer.materials.size()) {
        return CommandResult::fail("scene.remove_material_slot requires a valid index");
    }

    const auto removedMaterial = meshRenderer.materials[*index];
    meshRenderer.materials.erase(meshRenderer.materials.begin() + static_cast<std::ptrdiff_t>(*index));

    auto result = CommandResult::ok("Material slot removed");
    result.set("entity", entityToValue(*entity));
    result.set("index", static_cast<uint64_t>(*index));
    result.set("removed_material", removedMaterial);
    return result;
}

CommandResult addScriptBinding(ToolContext& context, const CommandArgs& args)
{
    auto* scene = activeScene(context);
    if (scene == nullptr) {
        return CommandResult::fail("scene.add_script_binding requires an active scene");
    }

    const auto entity = entityArg(args, "entity");
    if (!entity || !scene->isValidEntity(*entity)) {
        return CommandResult::fail("scene.add_script_binding requires a valid entity");
    }
    if (!scene->hasComponent<scene::ScriptComponent>(*entity)) {
        return CommandResult::fail("scene.add_script_binding requires Script component");
    }

    auto& script = scene->getComponent<scene::ScriptComponent>(*entity);
    const auto binding = scene::ScriptBinding{
        .script = assetArg(args, "script"),
        .enabled = boolArg(args, "enabled", true),
    };
    const auto index = indexArg(args, "index").value_or(script.scripts.size());
    if (index > script.scripts.size()) {
        return CommandResult::fail("scene.add_script_binding index is out of range");
    }

    script.scripts.insert(script.scripts.begin() + static_cast<std::ptrdiff_t>(index), binding);

    auto result = CommandResult::ok("Script binding added");
    result.set("entity", entityToValue(*entity));
    result.set("index", static_cast<uint64_t>(index));
    result.set("script", binding.script);
    result.set("enabled", binding.enabled);
    return result;
}

CommandResult removeScriptBinding(ToolContext& context, const CommandArgs& args)
{
    auto* scene = activeScene(context);
    if (scene == nullptr) {
        return CommandResult::fail("scene.remove_script_binding requires an active scene");
    }

    const auto entity = entityArg(args, "entity");
    if (!entity || !scene->isValidEntity(*entity)) {
        return CommandResult::fail("scene.remove_script_binding requires a valid entity");
    }
    if (!scene->hasComponent<scene::ScriptComponent>(*entity)) {
        return CommandResult::fail("scene.remove_script_binding requires Script component");
    }

    auto& script = scene->getComponent<scene::ScriptComponent>(*entity);
    const auto index = indexArg(args, "index");
    if (!index || *index >= script.scripts.size()) {
        return CommandResult::fail("scene.remove_script_binding requires a valid index");
    }

    const auto removedBinding = script.scripts[*index];
    script.scripts.erase(script.scripts.begin() + static_cast<std::ptrdiff_t>(*index));

    auto result = CommandResult::ok("Script binding removed");
    result.set("entity", entityToValue(*entity));
    result.set("index", static_cast<uint64_t>(*index));
    result.set("removed_script", removedBinding.script);
    result.set("removed_enabled", removedBinding.enabled);
    return result;
}

CommandResult editTag(ToolContext& context, const CommandArgs& args)
{
    scene::Entity entity;
    scene::TagComponent* tag = nullptr;
    auto result = resolveEditableComponent(context, args, EditTagCommandId, entity, tag);
    if (!result.success) {
        return result;
    }

    if (const auto value = args.get<std::string>("tag")) {
        tag->tag = *value;
    }

    return editResult("Tag edited", entity);
}

CommandResult editTransform(ToolContext& context, const CommandArgs& args)
{
    scene::Entity entity;
    scene::TransformComponent* transform = nullptr;
    auto result = resolveEditableComponent(context, args, EditTransformCommandId, entity, transform);
    if (!result.success) {
        return result;
    }

    applyTransformArgs(*transform, args);
    return editResult("Transform edited", entity);
}

CommandResult editMeshRenderer(ToolContext& context, const CommandArgs& args)
{
    scene::Entity entity;
    scene::MeshRendererComponent* meshRenderer = nullptr;
    auto result = resolveEditableComponent(context, args, EditMeshRendererCommandId, entity, meshRenderer);
    if (!result.success) {
        return result;
    }

    const auto materialIndex = indexArg(args, "material_index");
    if (args.contains("material") && !materialIndex) {
        return CommandResult::fail("scene.edit_mesh_renderer requires material_index when material is set");
    }
    if (materialIndex && *materialIndex >= meshRenderer->materials.size()) {
        return CommandResult::fail("scene.edit_mesh_renderer requires a valid material_index");
    }

    applyMeshRendererArgs(*meshRenderer, args);
    return editResult("Mesh renderer edited", entity);
}

CommandResult editSpriteRenderer(ToolContext& context, const CommandArgs& args)
{
    scene::Entity entity;
    scene::SpriteRendererComponent* spriteRenderer = nullptr;
    auto result = resolveEditableComponent(context, args, EditSpriteRendererCommandId, entity, spriteRenderer);
    if (!result.success) {
        return result;
    }

    applySpriteRendererArgs(*spriteRenderer, args);
    return editResult("Sprite renderer edited", entity);
}

CommandResult editScript(ToolContext& context, const CommandArgs& args)
{
    scene::Entity entity;
    scene::ScriptComponent* script = nullptr;
    auto result = resolveEditableComponent(context, args, EditScriptCommandId, entity, script);
    if (!result.success) {
        return result;
    }

    const auto index = indexArg(args, "index");
    if (!index || *index >= script->scripts.size()) {
        return CommandResult::fail("scene.edit_script requires a valid index");
    }

    auto& binding = script->scripts[*index];
    binding.script = assetArg(args, "script", binding.script);
    if (const auto value = args.get<bool>("enabled")) {
        binding.enabled = *value;
    }

    auto edit = editResult("Script binding edited", entity);
    edit.set("index", static_cast<uint64_t>(*index));
    return edit;
}

CommandResult editCamera(ToolContext& context, const CommandArgs& args)
{
    scene::Entity entity;
    scene::CameraComponent* camera = nullptr;
    auto result = resolveEditableComponent(context, args, EditCameraCommandId, entity, camera);
    if (!result.success) {
        return result;
    }

    applyCameraArgs(*camera, args);
    return editResult("Camera edited", entity);
}

CommandResult editDirectionalLight(ToolContext& context, const CommandArgs& args)
{
    scene::Entity entity;
    scene::DirectionalLightComponent* light = nullptr;
    auto result = resolveEditableComponent(context, args, EditDirectionalLightCommandId, entity, light);
    if (!result.success) {
        return result;
    }

    applyDirectionalLightArgs(*light, args);
    return editResult("Directional light edited", entity);
}

CommandResult editSceneSettings(ToolContext& context, const CommandArgs& args)
{
    auto* scene = activeScene(context);
    if (scene == nullptr) {
        return CommandResult::fail("scene.edit_scene_settings requires an active scene");
    }

    auto& settings = scene->getSettings();
    settings.environment_map = assetArg(args, "environment_map", settings.environment_map);
    if (const auto value = floatArg(args, "environment_intensity")) {
        settings.environment_intensity = std::max(*value, 0.0f);
    }

    return CommandResult::ok("Scene settings edited");
}

} // namespace

std::string_view CreateEntityCommand::id() const
{
    return CreateEntityCommandId;
}

std::string_view CreateEntityCommand::label() const
{
    return "Create Entity";
}

std::string_view CreateEntityCommand::category() const
{
    return "Scene";
}

bool CreateEntityCommand::canUndo() const
{
    return true;
}

CommandResult CreateEntityCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return createEntity(context, args);
}

std::string_view DeleteEntityCommand::id() const
{
    return DeleteEntityCommandId;
}

std::string_view DeleteEntityCommand::label() const
{
    return "Delete Entity";
}

std::string_view DeleteEntityCommand::category() const
{
    return "Scene";
}

bool DeleteEntityCommand::canUndo() const
{
    return true;
}

CommandResult DeleteEntityCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return deleteEntity(context, args);
}

std::string_view SetParentCommand::id() const
{
    return SetParentCommandId;
}

std::string_view SetParentCommand::label() const
{
    return "Set Parent";
}

std::string_view SetParentCommand::category() const
{
    return "Scene";
}

bool SetParentCommand::canUndo() const
{
    return true;
}

CommandResult SetParentCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return setParent(context, args);
}

std::string_view ClearParentCommand::id() const
{
    return ClearParentCommandId;
}

std::string_view ClearParentCommand::label() const
{
    return "Clear Parent";
}

std::string_view ClearParentCommand::category() const
{
    return "Scene";
}

bool ClearParentCommand::canUndo() const
{
    return true;
}

CommandResult ClearParentCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return clearParent(context, args);
}

std::string_view CreateEntityFromAssetCommand::id() const
{
    return CreateEntityFromAssetCommandId;
}

std::string_view CreateEntityFromAssetCommand::label() const
{
    return "Create Entity From Asset";
}

std::string_view CreateEntityFromAssetCommand::category() const
{
    return "Scene";
}

bool CreateEntityFromAssetCommand::canUndo() const
{
    return true;
}

CommandResult CreateEntityFromAssetCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return createEntityFromAsset(context, args);
}

std::string_view CreateBuiltinMeshEntityCommand::id() const
{
    return CreateBuiltinMeshEntityCommandId;
}

std::string_view CreateBuiltinMeshEntityCommand::label() const
{
    return "Create Builtin Mesh Entity";
}

std::string_view CreateBuiltinMeshEntityCommand::category() const
{
    return "Scene";
}

bool CreateBuiltinMeshEntityCommand::canUndo() const
{
    return true;
}

CommandResult CreateBuiltinMeshEntityCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return createBuiltinMeshEntity(context, args);
}

std::string_view AddComponentCommand::id() const
{
    return AddComponentCommandId;
}

std::string_view AddComponentCommand::label() const
{
    return "Add Component";
}

std::string_view AddComponentCommand::category() const
{
    return "Scene";
}

bool AddComponentCommand::canUndo() const
{
    return true;
}

CommandResult AddComponentCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return addComponent(context, args);
}

std::string_view RemoveComponentCommand::id() const
{
    return RemoveComponentCommandId;
}

std::string_view RemoveComponentCommand::label() const
{
    return "Remove Component";
}

std::string_view RemoveComponentCommand::category() const
{
    return "Scene";
}

bool RemoveComponentCommand::canUndo() const
{
    return true;
}

CommandResult RemoveComponentCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return removeComponent(context, args);
}

std::string_view AddMaterialSlotCommand::id() const
{
    return AddMaterialSlotCommandId;
}

std::string_view AddMaterialSlotCommand::label() const
{
    return "Add Material Slot";
}

std::string_view AddMaterialSlotCommand::category() const
{
    return "Scene";
}

bool AddMaterialSlotCommand::canUndo() const
{
    return true;
}

CommandResult AddMaterialSlotCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return addMaterialSlot(context, args);
}

std::string_view RemoveMaterialSlotCommand::id() const
{
    return RemoveMaterialSlotCommandId;
}

std::string_view RemoveMaterialSlotCommand::label() const
{
    return "Remove Material Slot";
}

std::string_view RemoveMaterialSlotCommand::category() const
{
    return "Scene";
}

bool RemoveMaterialSlotCommand::canUndo() const
{
    return true;
}

CommandResult RemoveMaterialSlotCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return removeMaterialSlot(context, args);
}

std::string_view AddScriptBindingCommand::id() const
{
    return AddScriptBindingCommandId;
}

std::string_view AddScriptBindingCommand::label() const
{
    return "Add Script Binding";
}

std::string_view AddScriptBindingCommand::category() const
{
    return "Scene";
}

bool AddScriptBindingCommand::canUndo() const
{
    return true;
}

CommandResult AddScriptBindingCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return addScriptBinding(context, args);
}

std::string_view RemoveScriptBindingCommand::id() const
{
    return RemoveScriptBindingCommandId;
}

std::string_view RemoveScriptBindingCommand::label() const
{
    return "Remove Script Binding";
}

std::string_view RemoveScriptBindingCommand::category() const
{
    return "Scene";
}

bool RemoveScriptBindingCommand::canUndo() const
{
    return true;
}

CommandResult RemoveScriptBindingCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return removeScriptBinding(context, args);
}

std::string_view EditTagCommand::id() const
{
    return EditTagCommandId;
}

std::string_view EditTagCommand::label() const
{
    return "Edit Tag";
}

std::string_view EditTagCommand::category() const
{
    return "Scene";
}

bool EditTagCommand::canUndo() const
{
    return true;
}

CommandResult EditTagCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return editTag(context, args);
}

std::string_view EditTransformCommand::id() const
{
    return EditTransformCommandId;
}

std::string_view EditTransformCommand::label() const
{
    return "Edit Transform";
}

std::string_view EditTransformCommand::category() const
{
    return "Scene";
}

bool EditTransformCommand::canUndo() const
{
    return true;
}

CommandResult EditTransformCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return editTransform(context, args);
}

std::string_view EditMeshRendererCommand::id() const
{
    return EditMeshRendererCommandId;
}

std::string_view EditMeshRendererCommand::label() const
{
    return "Edit Mesh Renderer";
}

std::string_view EditMeshRendererCommand::category() const
{
    return "Scene";
}

bool EditMeshRendererCommand::canUndo() const
{
    return true;
}

CommandResult EditMeshRendererCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return editMeshRenderer(context, args);
}

std::string_view EditSpriteRendererCommand::id() const
{
    return EditSpriteRendererCommandId;
}

std::string_view EditSpriteRendererCommand::label() const
{
    return "Edit Sprite Renderer";
}

std::string_view EditSpriteRendererCommand::category() const
{
    return "Scene";
}

bool EditSpriteRendererCommand::canUndo() const
{
    return true;
}

CommandResult EditSpriteRendererCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return editSpriteRenderer(context, args);
}

std::string_view EditScriptCommand::id() const
{
    return EditScriptCommandId;
}

std::string_view EditScriptCommand::label() const
{
    return "Edit Script";
}

std::string_view EditScriptCommand::category() const
{
    return "Scene";
}

bool EditScriptCommand::canUndo() const
{
    return true;
}

CommandResult EditScriptCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return editScript(context, args);
}

std::string_view EditCameraCommand::id() const
{
    return EditCameraCommandId;
}

std::string_view EditCameraCommand::label() const
{
    return "Edit Camera";
}

std::string_view EditCameraCommand::category() const
{
    return "Scene";
}

bool EditCameraCommand::canUndo() const
{
    return true;
}

CommandResult EditCameraCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return editCamera(context, args);
}

std::string_view EditDirectionalLightCommand::id() const
{
    return EditDirectionalLightCommandId;
}

std::string_view EditDirectionalLightCommand::label() const
{
    return "Edit Directional Light";
}

std::string_view EditDirectionalLightCommand::category() const
{
    return "Scene";
}

bool EditDirectionalLightCommand::canUndo() const
{
    return true;
}

CommandResult EditDirectionalLightCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return editDirectionalLight(context, args);
}

std::string_view EditSceneSettingsCommand::id() const
{
    return EditSceneSettingsCommandId;
}

std::string_view EditSceneSettingsCommand::label() const
{
    return "Edit Scene Settings";
}

std::string_view EditSceneSettingsCommand::category() const
{
    return "Scene";
}

bool EditSceneSettingsCommand::canUndo() const
{
    return true;
}

CommandResult EditSceneSettingsCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return editSceneSettings(context, args);
}

void registerSceneCommands(CommandRegistry& registry)
{
    registry.registerCommand(std::make_unique<CreateEntityCommand>());
    registry.registerCommand(std::make_unique<DeleteEntityCommand>());
    registry.registerCommand(std::make_unique<SetParentCommand>());
    registry.registerCommand(std::make_unique<ClearParentCommand>());
    registry.registerCommand(std::make_unique<CreateEntityFromAssetCommand>());
    registry.registerCommand(std::make_unique<CreateBuiltinMeshEntityCommand>());
    registry.registerCommand(std::make_unique<AddComponentCommand>());
    registry.registerCommand(std::make_unique<RemoveComponentCommand>());
    registry.registerCommand(std::make_unique<AddMaterialSlotCommand>());
    registry.registerCommand(std::make_unique<RemoveMaterialSlotCommand>());
    registry.registerCommand(std::make_unique<AddScriptBindingCommand>());
    registry.registerCommand(std::make_unique<RemoveScriptBindingCommand>());
    registry.registerCommand(std::make_unique<EditTagCommand>());
    registry.registerCommand(std::make_unique<EditTransformCommand>());
    registry.registerCommand(std::make_unique<EditMeshRendererCommand>());
    registry.registerCommand(std::make_unique<EditSpriteRendererCommand>());
    registry.registerCommand(std::make_unique<EditScriptCommand>());
    registry.registerCommand(std::make_unique<EditCameraCommand>());
    registry.registerCommand(std::make_unique<EditDirectionalLightCommand>());
    registry.registerCommand(std::make_unique<EditSceneSettingsCommand>());
}

} // namespace lunalite::tooling
