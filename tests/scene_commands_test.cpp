#include "../LunaLite/asset/asset_manager.h"
#include "../LunaLite/asset/builtin/builtin_assets.h"
#include "../LunaLite/project/project_manager.h"
#include "../LunaLite/scene/components.h"
#include "../LunaLite/scene/scene.h"
#include "../LunaLiteTooling/commands/command_registry.h"
#include "../LunaLiteTooling/commands/scene_commands.h"
#include "../LunaLiteTooling/context/tool_context.h"
#include "../third_party/stb/stb_image_write.h"

#include <cstdint>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>

namespace {
using EntityUnderlying = std::underlying_type_t<entt::entity>;

uint64_t entityToValue(lunalite::scene::Entity entity)
{
    return static_cast<uint64_t>(static_cast<EntityUnderlying>(entity.getHandle()));
}

lunalite::scene::Entity entityFromValue(uint64_t value)
{
    return lunalite::scene::Entity{static_cast<entt::entity>(static_cast<EntityUnderlying>(value))};
}

lunalite::scene::Entity createdEntityFrom(const lunalite::tooling::CommandResult& result)
{
    const auto* value = result.get<uint64_t>("created_entity");
    return value != nullptr ? entityFromValue(*value) : lunalite::scene::Entity{};
}

lunalite::scene::Entity affectedEntityFrom(const lunalite::tooling::CommandResult& result)
{
    const auto* value = result.get<uint64_t>("affected_entity");
    return value != nullptr ? entityFromValue(*value) : lunalite::scene::Entity{};
}

bool writeTestPngTexture(const std::filesystem::path& path)
{
    constexpr std::array<unsigned char, 16> pixels{
        255,
        0,
        0,
        255,
        0,
        255,
        0,
        255,
        0,
        0,
        255,
        255,
        255,
        255,
        255,
        255,
    };
    return stbi_write_png(path.string().c_str(), 2, 2, 4, pixels.data(), 2 * 4) != 0;
}
} // namespace

int main()
{
    using namespace lunalite;

    tooling::ToolContext missingContext;
    const auto missingSceneResult =
        tooling::CommandRegistry::get().execute(tooling::CreateEntityCommandId, missingContext);
    if (missingSceneResult.success) {
        std::cerr << "scene.create_entity should fail without an active scene.\n";
        return 1;
    }

    scene::Scene scene;
    tooling::ToolContext context;
    context.setScene(scene);

    tooling::CommandArgs rootArgs;
    rootArgs.set("name", std::string{"Root"});
    const auto rootResult = tooling::CommandRegistry::get().execute(tooling::CreateEntityCommandId, context, rootArgs);
    const auto root = createdEntityFrom(rootResult);
    if (!rootResult.success || !scene.isValidEntity(root)) {
        std::cerr << "scene.create_entity did not create a valid entity.\n";
        return 1;
    }
    if (!scene.hasComponent<scene::TagComponent>(root) || scene.getComponent<scene::TagComponent>(root).tag != "Root") {
        std::cerr << "scene.create_entity did not apply the requested name.\n";
        return 1;
    }

    tooling::CommandArgs childArgs;
    childArgs.set("name", std::string{"Child"});
    childArgs.set("parent_entity", entityToValue(root));
    const auto childResult =
        tooling::CommandRegistry::get().execute(tooling::CreateEntityCommandId, context, childArgs);
    const auto child = createdEntityFrom(childResult);
    if (!childResult.success || scene.getParent(child).getHandle() != root.getHandle()) {
        std::cerr << "scene.create_entity did not parent the child entity.\n";
        return 1;
    }

    const auto otherResult = tooling::CommandRegistry::get().execute(tooling::CreateEntityCommandId, context);
    const auto other = createdEntityFrom(otherResult);
    if (!otherResult.success || !scene.isValidEntity(other)) {
        std::cerr << "scene.create_entity did not create the second entity.\n";
        return 1;
    }

    tooling::CommandArgs setParentArgs;
    setParentArgs.set("entity", entityToValue(child));
    setParentArgs.set("parent_entity", entityToValue(other));
    setParentArgs.set("keep_world_transform", true);
    const auto setParentResult =
        tooling::CommandRegistry::get().execute(tooling::SetParentCommandId, context, setParentArgs);
    if (!setParentResult.success || scene.getParent(child).getHandle() != other.getHandle()) {
        std::cerr << "scene.set_parent did not move the child entity.\n";
        return 1;
    }

    tooling::CommandArgs invalidParentArgs;
    invalidParentArgs.set("entity", entityToValue(child));
    invalidParentArgs.set("parent_entity", entityToValue(child));
    if (tooling::CommandRegistry::get().execute(tooling::SetParentCommandId, context, invalidParentArgs).success) {
        std::cerr << "scene.set_parent should reject self-parenting.\n";
        return 1;
    }

    tooling::CommandArgs clearParentArgs;
    clearParentArgs.set("entity", entityToValue(child));
    clearParentArgs.set("keep_world_transform", true);
    const auto clearParentResult =
        tooling::CommandRegistry::get().execute(tooling::ClearParentCommandId, context, clearParentArgs);
    if (!clearParentResult.success || scene.getParent(child)) {
        std::cerr << "scene.clear_parent did not clear the parent.\n";
        return 1;
    }

    tooling::CommandArgs deleteParentArgs;
    deleteParentArgs.set("entity", entityToValue(other));
    const auto deleteParentResult =
        tooling::CommandRegistry::get().execute(tooling::DeleteEntityCommandId, context, deleteParentArgs);
    if (!deleteParentResult.success || scene.isValidEntity(other)) {
        std::cerr << "scene.delete_entity did not delete the requested entity.\n";
        return 1;
    }

    if (tooling::CommandRegistry::get().execute(tooling::DeleteEntityCommandId, context, deleteParentArgs).success) {
        std::cerr << "scene.delete_entity should reject an invalid entity.\n";
        return 1;
    }

    tooling::CommandArgs builtinMeshArgs;
    builtinMeshArgs.set("name", std::string{"Builtin Cube"});
    builtinMeshArgs.set("mesh", asset::builtin::cubeMeshHandle());
    builtinMeshArgs.set("material", asset::builtin::defaultMaterialHandle());
    const auto builtinMeshResult =
        tooling::CommandRegistry::get().execute(tooling::CreateBuiltinMeshEntityCommandId, context, builtinMeshArgs);
    const auto builtinMeshEntity = createdEntityFrom(builtinMeshResult);
    if (!builtinMeshResult.success || !scene.isValidEntity(builtinMeshEntity) ||
        !scene.hasComponent<scene::MeshRendererComponent>(builtinMeshEntity) ||
        scene.getComponent<scene::MeshRendererComponent>(builtinMeshEntity).mesh != asset::builtin::cubeMeshHandle() ||
        scene.getComponent<scene::MeshRendererComponent>(builtinMeshEntity).materials.size() != 1 ||
        scene.getComponent<scene::MeshRendererComponent>(builtinMeshEntity).materials.front() !=
            asset::builtin::defaultMaterialHandle() ||
        scene.getComponent<scene::TagComponent>(builtinMeshEntity).tag != "Builtin Cube") {
        std::cerr << "scene.create_builtin_mesh_entity did not create the expected mesh renderer entity.\n";
        return 1;
    }

    const auto componentEntity = scene.createEntity();
    tooling::CommandArgs addSpriteComponentArgs;
    addSpriteComponentArgs.set("entity", entityToValue(componentEntity));
    addSpriteComponentArgs.set("component_type", std::string{tooling::SpriteRendererComponentType});
    if (!tooling::CommandRegistry::get()
             .execute(tooling::AddComponentCommandId, context, addSpriteComponentArgs)
             .success ||
        !scene.hasComponent<scene::SpriteRendererComponent>(componentEntity)) {
        std::cerr << "scene.add_component did not add SpriteRenderer.\n";
        return 1;
    }
    if (tooling::CommandRegistry::get()
            .execute(tooling::AddComponentCommandId, context, addSpriteComponentArgs)
            .success) {
        std::cerr << "scene.add_component should reject duplicate components.\n";
        return 1;
    }
    tooling::CommandArgs removeSpriteComponentArgs;
    removeSpriteComponentArgs.set("entity", entityToValue(componentEntity));
    removeSpriteComponentArgs.set("component_type", std::string{tooling::SpriteRendererComponentType});
    if (!tooling::CommandRegistry::get()
             .execute(tooling::RemoveComponentCommandId, context, removeSpriteComponentArgs)
             .success ||
        scene.hasComponent<scene::SpriteRendererComponent>(componentEntity)) {
        std::cerr << "scene.remove_component did not remove SpriteRenderer.\n";
        return 1;
    }
    if (tooling::CommandRegistry::get()
            .execute(tooling::RemoveComponentCommandId, context, removeSpriteComponentArgs)
            .success) {
        std::cerr << "scene.remove_component should reject missing components.\n";
        return 1;
    }

    tooling::CommandArgs addMeshRendererArgs;
    addMeshRendererArgs.set("entity", entityToValue(componentEntity));
    addMeshRendererArgs.set("component_type", std::string{tooling::MeshRendererComponentType});
    if (!tooling::CommandRegistry::get()
             .execute(tooling::AddComponentCommandId, context, addMeshRendererArgs)
             .success) {
        std::cerr << "scene.add_component did not add MeshRenderer.\n";
        return 1;
    }

    tooling::CommandArgs addMaterialSlotArgs;
    addMaterialSlotArgs.set("entity", entityToValue(componentEntity));
    addMaterialSlotArgs.set("material", asset::builtin::defaultMaterialHandle());
    if (!tooling::CommandRegistry::get()
             .execute(tooling::AddMaterialSlotCommandId, context, addMaterialSlotArgs)
             .success ||
        scene.getComponent<scene::MeshRendererComponent>(componentEntity).materials.size() != 1) {
        std::cerr << "scene.add_material_slot did not add a material.\n";
        return 1;
    }

    tooling::CommandArgs removeMaterialSlotArgs;
    removeMaterialSlotArgs.set("entity", entityToValue(componentEntity));
    removeMaterialSlotArgs.set("index", uint64_t{0});
    if (!tooling::CommandRegistry::get()
             .execute(tooling::RemoveMaterialSlotCommandId, context, removeMaterialSlotArgs)
             .success ||
        !scene.getComponent<scene::MeshRendererComponent>(componentEntity).materials.empty()) {
        std::cerr << "scene.remove_material_slot did not remove the material.\n";
        return 1;
    }
    if (tooling::CommandRegistry::get()
            .execute(tooling::RemoveMaterialSlotCommandId, context, removeMaterialSlotArgs)
            .success) {
        std::cerr << "scene.remove_material_slot should reject an invalid index.\n";
        return 1;
    }

    tooling::CommandArgs addScriptComponentArgs;
    addScriptComponentArgs.set("entity", entityToValue(componentEntity));
    addScriptComponentArgs.set("component_type", std::string{tooling::ScriptComponentType});
    if (!tooling::CommandRegistry::get()
             .execute(tooling::AddComponentCommandId, context, addScriptComponentArgs)
             .success) {
        std::cerr << "scene.add_component did not add Script component.\n";
        return 1;
    }

    tooling::CommandArgs addScriptBindingArgs;
    addScriptBindingArgs.set("entity", entityToValue(componentEntity));
    addScriptBindingArgs.set("enabled", false);
    if (!tooling::CommandRegistry::get()
             .execute(tooling::AddScriptBindingCommandId, context, addScriptBindingArgs)
             .success ||
        scene.getComponent<scene::ScriptComponent>(componentEntity).scripts.size() != 1 ||
        scene.getComponent<scene::ScriptComponent>(componentEntity).scripts.front().enabled) {
        std::cerr << "scene.add_script_binding did not add the expected binding.\n";
        return 1;
    }

    tooling::CommandArgs removeScriptBindingArgs;
    removeScriptBindingArgs.set("entity", entityToValue(componentEntity));
    removeScriptBindingArgs.set("index", uint64_t{0});
    if (!tooling::CommandRegistry::get()
             .execute(tooling::RemoveScriptBindingCommandId, context, removeScriptBindingArgs)
             .success ||
        !scene.getComponent<scene::ScriptComponent>(componentEntity).scripts.empty()) {
        std::cerr << "scene.remove_script_binding did not remove the binding.\n";
        return 1;
    }
    if (tooling::CommandRegistry::get()
            .execute(tooling::RemoveScriptBindingCommandId, context, removeScriptBindingArgs)
            .success) {
        std::cerr << "scene.remove_script_binding should reject an invalid index.\n";
        return 1;
    }

    const auto projectRoot = std::filesystem::current_path() / "build" / "scene_commands_test_project";
    std::error_code error;
    std::filesystem::remove_all(projectRoot, error);

    project::ProjectInfo info;
    info.name = "SceneCommandsTestProject";
    info.assets_path = "Assets";
    if (!project::ProjectManager::instance().createProject(projectRoot, info)) {
        std::cerr << "Failed to create test project.\n";
        return 1;
    }

    const auto texturePath = projectRoot / info.assets_path / "texture.png";
    if (!writeTestPngTexture(texturePath)) {
        std::cerr << "Failed to write test texture.\n";
        return 1;
    }

    const auto scriptPath = projectRoot / info.assets_path / "rotate.lua";
    {
        std::ofstream script(scriptPath);
        script << "function update(dt)\nend\n";
    }

    if (!asset::AssetManager::get().loadProjectAssets()) {
        std::cerr << "Failed to load test project assets.\n";
        return 1;
    }

    const auto textureHandle = asset::AssetManager::get().getHandleByRelativePath("Assets/texture.png");
    const auto scriptHandle = asset::AssetManager::get().getHandleByRelativePath("Assets/rotate.lua");
    if (!textureHandle.isValid() || !scriptHandle.isValid()) {
        std::cerr << "Failed to resolve test asset handles.\n";
        return 1;
    }

    const auto spritePath = projectRoot / info.assets_path / "texture.lunasprite";
    {
        std::ofstream sprite(spritePath);
        sprite << "Sprite:\n";
        sprite << "  Texture: " << static_cast<uint64_t>(textureHandle) << "\n";
        sprite << "  SourceRect:\n";
        sprite << "    X: 0\n";
        sprite << "    Y: 0\n";
        sprite << "    Width: 2\n";
        sprite << "    Height: 2\n";
        sprite << "  Pivot:\n";
        sprite << "    X: 0.5\n";
        sprite << "    Y: 0.5\n";
        sprite << "  PixelsPerUnit: 100\n";
        sprite << "  FlipY: true\n";
        sprite << "  ColorSpace: SRGB\n";
    }

    const auto spriteHandle = asset::AssetManager::get().importAndLoadAsset(spritePath);
    if (!spriteHandle || !spriteHandle->isValid()) {
        std::cerr << "Failed to import test sprite asset.\n";
        return 1;
    }

    tooling::CommandArgs spriteEntityArgs;
    spriteEntityArgs.set("source_asset", *spriteHandle);
    const auto spriteEntityResult =
        tooling::CommandRegistry::get().execute(tooling::CreateEntityFromAssetCommandId, context, spriteEntityArgs);
    const auto spriteEntity = createdEntityFrom(spriteEntityResult);
    if (!spriteEntityResult.success || !scene.isValidEntity(spriteEntity) ||
        !scene.hasComponent<scene::SpriteRendererComponent>(spriteEntity) ||
        scene.getComponent<scene::SpriteRendererComponent>(spriteEntity).sprite != *spriteHandle ||
        scene.getComponent<scene::TagComponent>(spriteEntity).tag != "texture") {
        std::cerr << "scene.create_entity_from_asset did not create a sprite renderer entity.\n";
        return 1;
    }

    const auto scriptTarget = scene.createEntity();
    tooling::CommandArgs scriptTargetArgs;
    scriptTargetArgs.set("source_asset", scriptHandle);
    scriptTargetArgs.set("target_entity", entityToValue(scriptTarget));
    const auto scriptTargetResult =
        tooling::CommandRegistry::get().execute(tooling::CreateEntityFromAssetCommandId, context, scriptTargetArgs);
    const auto affectedEntity = affectedEntityFrom(scriptTargetResult);
    if (!scriptTargetResult.success || affectedEntity.getHandle() != scriptTarget.getHandle() ||
        !scene.hasComponent<scene::ScriptComponent>(scriptTarget) ||
        scene.getComponent<scene::ScriptComponent>(scriptTarget).scripts.empty() ||
        scene.getComponent<scene::ScriptComponent>(scriptTarget).scripts.front().script != scriptHandle) {
        std::cerr << "scene.create_entity_from_asset did not add script to target entity.\n";
        return 1;
    }

    tooling::CommandArgs scriptEntityArgs;
    scriptEntityArgs.set("source_asset", scriptHandle);
    const auto scriptEntityResult =
        tooling::CommandRegistry::get().execute(tooling::CreateEntityFromAssetCommandId, context, scriptEntityArgs);
    const auto scriptEntity = createdEntityFrom(scriptEntityResult);
    if (!scriptEntityResult.success || !scene.isValidEntity(scriptEntity) ||
        !scene.hasComponent<scene::ScriptComponent>(scriptEntity) ||
        scene.getComponent<scene::ScriptComponent>(scriptEntity).scripts.empty() ||
        scene.getComponent<scene::ScriptComponent>(scriptEntity).scripts.front().script != scriptHandle ||
        scene.getComponent<scene::TagComponent>(scriptEntity).tag != "rotate") {
        std::cerr << "scene.create_entity_from_asset did not create a script entity.\n";
        return 1;
    }

    tooling::CommandArgs unsupportedAssetArgs;
    unsupportedAssetArgs.set("source_asset", textureHandle);
    if (tooling::CommandRegistry::get()
            .execute(tooling::CreateEntityFromAssetCommandId, context, unsupportedAssetArgs)
            .success) {
        std::cerr << "scene.create_entity_from_asset should reject unsupported asset types.\n";
        return 1;
    }

    std::cout << "Scene commands create, delete, reparent, and create entities from assets.\n";
    return 0;
}
