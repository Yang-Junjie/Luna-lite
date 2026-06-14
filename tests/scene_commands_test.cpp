#include "../LunaLite/asset/asset_manager.h"
#include "../LunaLite/asset/builtin/builtin_assets.h"
#include "../LunaLite/project/project_manager.h"
#include "../LunaLite/renderer/default_renderer/components/renderer_components.h"
#include "../LunaLite/renderer/interface/camera.h"
#include "../LunaLite/scene/scene.h"
#include "../LunaLite/scene/scene_components.h"
#include "../LunaLite/script/script_components.h"
#include "../LunaLiteTooling/commands/command_manager.h"
#include "../LunaLiteTooling/commands/command_registry.h"
#include "../LunaLiteTooling/commands/scene_commands.h"
#include "../LunaLiteTooling/context/tool_context.h"
#include "../third_party/stb/stb_image_write.h"

#include <cmath>
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

bool nearlyEqual(float lhs, float rhs)
{
    return std::abs(lhs - rhs) <= 0.001f;
}

bool nearlyEqual(const glm::vec3& lhs, const glm::vec3& rhs)
{
    return nearlyEqual(lhs.x, rhs.x) && nearlyEqual(lhs.y, rhs.y) && nearlyEqual(lhs.z, rhs.z);
}

bool nearlyEqual(const glm::vec4& lhs, const glm::vec4& rhs)
{
    return nearlyEqual(lhs.x, rhs.x) && nearlyEqual(lhs.y, rhs.y) && nearlyEqual(lhs.z, rhs.z) &&
           nearlyEqual(lhs.w, rhs.w);
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
        scene.getComponent<scene::ScriptComponent>(componentEntity).scripts.front().script != asset::AssetHandle{0} ||
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

    tooling::CommandArgs editTagArgs;
    editTagArgs.set("entity", entityToValue(root));
    editTagArgs.set("tag", std::string{"Edited Root"});
    if (!tooling::CommandRegistry::get().execute(tooling::EditTagCommandId, context, editTagArgs).success ||
        scene.getComponent<scene::TagComponent>(root).tag != "Edited Root") {
        std::cerr << "scene.edit_tag did not update the tag.\n";
        return 1;
    }

    tooling::CommandArgs editTransformArgs;
    editTransformArgs.set("entity", entityToValue(root));
    editTransformArgs.set("translation_x", 1.0);
    editTransformArgs.set("translation_y", 2.0);
    editTransformArgs.set("translation_z", 3.0);
    editTransformArgs.set("rotation_degrees_y", 90.0);
    editTransformArgs.set("scale_x", 2.0);
    editTransformArgs.set("scale_y", 3.0);
    editTransformArgs.set("scale_z", 4.0);
    if (!tooling::CommandRegistry::get().execute(tooling::EditTransformCommandId, context, editTransformArgs).success) {
        std::cerr << "scene.edit_transform failed.\n";
        return 1;
    }
    const auto& editedTransform = scene.getComponent<scene::TransformComponent>(root);
    if (!nearlyEqual(editedTransform.translation, glm::vec3{1.0f, 2.0f, 3.0f}) ||
        !nearlyEqual(editedTransform.scale, glm::vec3{2.0f, 3.0f, 4.0f})) {
        std::cerr << "scene.edit_transform did not update translation and scale.\n";
        return 1;
    }

    tooling::CommandArgs addMaterialForEditArgs;
    addMaterialForEditArgs.set("entity", entityToValue(componentEntity));
    addMaterialForEditArgs.set("material", asset::builtin::defaultMaterialHandle());
    if (!tooling::CommandRegistry::get()
             .execute(tooling::AddMaterialSlotCommandId, context, addMaterialForEditArgs)
             .success) {
        std::cerr << "scene.add_material_slot did not prepare material edit coverage.\n";
        return 1;
    }

    tooling::CommandArgs editMeshRendererArgs;
    editMeshRendererArgs.set("entity", entityToValue(componentEntity));
    editMeshRendererArgs.set("mesh", asset::builtin::planeMeshHandle());
    editMeshRendererArgs.set("cast_shadow", false);
    editMeshRendererArgs.set("submesh_start", uint64_t{1});
    editMeshRendererArgs.set("submesh_count", uint64_t{2});
    editMeshRendererArgs.set("material_index", uint64_t{0});
    editMeshRendererArgs.set("material", asset::builtin::errorMaterialHandle());
    if (!tooling::CommandRegistry::get()
             .execute(tooling::EditMeshRendererCommandId, context, editMeshRendererArgs)
             .success) {
        std::cerr << "scene.edit_mesh_renderer failed.\n";
        return 1;
    }
    const auto& editedMeshRenderer = scene.getComponent<scene::MeshRendererComponent>(componentEntity);
    if (editedMeshRenderer.mesh != asset::builtin::planeMeshHandle() || editedMeshRenderer.cast_shadow ||
        editedMeshRenderer.submesh_start != 1 || editedMeshRenderer.submesh_count != 2 ||
        editedMeshRenderer.materials.size() != 1 ||
        editedMeshRenderer.materials.front() != asset::builtin::errorMaterialHandle()) {
        std::cerr << "scene.edit_mesh_renderer did not update mesh renderer fields.\n";
        return 1;
    }

    tooling::CommandArgs invalidMaterialEditArgs;
    invalidMaterialEditArgs.set("entity", entityToValue(componentEntity));
    invalidMaterialEditArgs.set("material", asset::builtin::defaultMaterialHandle());
    if (tooling::CommandRegistry::get()
            .execute(tooling::EditMeshRendererCommandId, context, invalidMaterialEditArgs)
            .success) {
        std::cerr << "scene.edit_mesh_renderer should require material_index when material is set.\n";
        return 1;
    }

    tooling::CommandArgs addSpriteForEditArgs;
    addSpriteForEditArgs.set("entity", entityToValue(componentEntity));
    addSpriteForEditArgs.set("component_type", std::string{tooling::SpriteRendererComponentType});
    if (!tooling::CommandRegistry::get()
             .execute(tooling::AddComponentCommandId, context, addSpriteForEditArgs)
             .success) {
        std::cerr << "scene.add_component did not prepare SpriteRenderer edit coverage.\n";
        return 1;
    }

    tooling::CommandArgs editSpriteRendererArgs;
    editSpriteRendererArgs.set("entity", entityToValue(componentEntity));
    editSpriteRendererArgs.set("sprite", asset::AssetHandle{77});
    editSpriteRendererArgs.set("color_r", 0.1);
    editSpriteRendererArgs.set("color_g", 0.2);
    editSpriteRendererArgs.set("color_b", 0.3);
    editSpriteRendererArgs.set("color_a", 0.4);
    editSpriteRendererArgs.set("sorting_layer", int64_t{-3});
    editSpriteRendererArgs.set("order_in_layer", int64_t{12});
    editSpriteRendererArgs.set("depth_test", true);
    if (!tooling::CommandRegistry::get()
             .execute(tooling::EditSpriteRendererCommandId, context, editSpriteRendererArgs)
             .success) {
        std::cerr << "scene.edit_sprite_renderer failed.\n";
        return 1;
    }
    const auto& editedSpriteRenderer = scene.getComponent<scene::SpriteRendererComponent>(componentEntity);
    if (editedSpriteRenderer.sprite != asset::AssetHandle{77} ||
        !nearlyEqual(editedSpriteRenderer.color, glm::vec4{0.1f, 0.2f, 0.3f, 0.4f}) ||
        editedSpriteRenderer.sorting_layer != -3 || editedSpriteRenderer.order_in_layer != 12 ||
        !editedSpriteRenderer.depth_test) {
        std::cerr << "scene.edit_sprite_renderer did not update sprite renderer fields.\n";
        return 1;
    }

    tooling::CommandArgs addScriptForEditArgs;
    addScriptForEditArgs.set("entity", entityToValue(componentEntity));
    addScriptForEditArgs.set("script", asset::AssetHandle{88});
    addScriptForEditArgs.set("enabled", true);
    if (!tooling::CommandRegistry::get()
             .execute(tooling::AddScriptBindingCommandId, context, addScriptForEditArgs)
             .success) {
        std::cerr << "scene.add_script_binding did not prepare script edit coverage.\n";
        return 1;
    }

    tooling::CommandArgs editScriptArgs;
    editScriptArgs.set("entity", entityToValue(componentEntity));
    editScriptArgs.set("index", uint64_t{0});
    editScriptArgs.set("script", asset::AssetHandle{99});
    editScriptArgs.set("enabled", false);
    if (!tooling::CommandRegistry::get().execute(tooling::EditScriptCommandId, context, editScriptArgs).success) {
        std::cerr << "scene.edit_script failed.\n";
        return 1;
    }
    const auto& editedScript = scene.getComponent<scene::ScriptComponent>(componentEntity);
    if (editedScript.scripts.size() != 1 || editedScript.scripts.front().script != asset::AssetHandle{99} ||
        editedScript.scripts.front().enabled) {
        std::cerr << "scene.edit_script did not update the script binding.\n";
        return 1;
    }

    tooling::CommandArgs invalidScriptEditArgs;
    invalidScriptEditArgs.set("entity", entityToValue(componentEntity));
    invalidScriptEditArgs.set("index", uint64_t{8});
    if (tooling::CommandRegistry::get().execute(tooling::EditScriptCommandId, context, invalidScriptEditArgs).success) {
        std::cerr << "scene.edit_script should reject an invalid index.\n";
        return 1;
    }

    tooling::CommandArgs addCameraForEditArgs;
    addCameraForEditArgs.set("entity", entityToValue(componentEntity));
    addCameraForEditArgs.set("component_type", std::string{tooling::CameraComponentType});
    if (!tooling::CommandRegistry::get()
             .execute(tooling::AddComponentCommandId, context, addCameraForEditArgs)
             .success) {
        std::cerr << "scene.add_component did not prepare Camera edit coverage.\n";
        return 1;
    }

    tooling::CommandArgs editCameraArgs;
    editCameraArgs.set("entity", entityToValue(componentEntity));
    editCameraArgs.set("primary", false);
    editCameraArgs.set("exposure", -2.0);
    editCameraArgs.set("projection_type",
                       static_cast<uint64_t>(renderer::interface::Camera::ProjectionType::Orthographic));
    if (!tooling::CommandRegistry::get().execute(tooling::EditCameraCommandId, context, editCameraArgs).success) {
        std::cerr << "scene.edit_camera failed.\n";
        return 1;
    }
    const auto& editedCamera = scene.getComponent<scene::CameraComponent>(componentEntity);
    if (editedCamera.primary || !nearlyEqual(editedCamera.camera.getExposure(), 0.0f) ||
        editedCamera.camera.getProjectionType() != renderer::interface::Camera::ProjectionType::Orthographic) {
        std::cerr << "scene.edit_camera did not update camera fields.\n";
        return 1;
    }

    tooling::CommandArgs addLightForEditArgs;
    addLightForEditArgs.set("entity", entityToValue(componentEntity));
    addLightForEditArgs.set("component_type", std::string{tooling::DirectionalLightComponentType});
    if (!tooling::CommandRegistry::get()
             .execute(tooling::AddComponentCommandId, context, addLightForEditArgs)
             .success) {
        std::cerr << "scene.add_component did not prepare DirectionalLight edit coverage.\n";
        return 1;
    }

    tooling::CommandArgs editLightArgs;
    editLightArgs.set("entity", entityToValue(componentEntity));
    editLightArgs.set("color_r", 0.25);
    editLightArgs.set("color_g", 0.5);
    editLightArgs.set("color_b", 0.75);
    editLightArgs.set("intensity", -5.0);
    editLightArgs.set("shadow_enabled", false);
    editLightArgs.set("shadow_map_size", uint64_t{0});
    editLightArgs.set("shadow_max_distance", -1.0);
    editLightArgs.set("shadow_bias", -2.0);
    editLightArgs.set("shadow_normal_bias", -3.0);
    editLightArgs.set("shadow_pcf_radius", uint64_t{8});
    editLightArgs.set("shadow_cascade_count", uint64_t{8});
    editLightArgs.set("shadow_cascade_split_lambda", 3.0);
    editLightArgs.set("shadow_cascade_seam_blend", -4.0);
    editLightArgs.set("shadow_cascade_caster_depth_padding", -5.0);
    if (!tooling::CommandRegistry::get()
             .execute(tooling::EditDirectionalLightCommandId, context, editLightArgs)
             .success) {
        std::cerr << "scene.edit_directional_light failed.\n";
        return 1;
    }
    const auto& editedLight = scene.getComponent<scene::DirectionalLightComponent>(componentEntity);
    if (!nearlyEqual(editedLight.color, glm::vec3{0.25f, 0.5f, 0.75f}) || !nearlyEqual(editedLight.intensity, 0.0f) ||
        editedLight.shadow.enabled || editedLight.shadow.map_size != 1 ||
        !nearlyEqual(editedLight.shadow.max_distance, 0.0f) || !nearlyEqual(editedLight.shadow.bias, 0.0f) ||
        !nearlyEqual(editedLight.shadow.normal_bias, 0.0f) || editedLight.shadow.pcf_radius != 4 ||
        editedLight.shadow.cascade_count != 4 || !nearlyEqual(editedLight.shadow.cascade_split_lambda, 1.0f) ||
        !nearlyEqual(editedLight.shadow.cascade_seam_blend, 0.0f) ||
        !nearlyEqual(editedLight.shadow.cascade_caster_depth_padding, 0.0f)) {
        std::cerr << "scene.edit_directional_light did not update and clamp light fields.\n";
        return 1;
    }

    tooling::CommandArgs editSceneSettingsArgs;
    editSceneSettingsArgs.set("environment_map", asset::AssetHandle{66});
    editSceneSettingsArgs.set("environment_intensity", -6.0);
    if (!tooling::CommandRegistry::get()
             .execute(tooling::EditSceneSettingsCommandId, context, editSceneSettingsArgs)
             .success ||
        scene.getSettings().environment_map != asset::AssetHandle{66} ||
        !nearlyEqual(scene.getSettings().environment_intensity, 0.0f)) {
        std::cerr << "scene.edit_scene_settings did not update scene settings.\n";
        return 1;
    }

    tooling::CommandManager::get().clearHistory();
    tooling::CommandArgs undoableEditTagArgs;
    undoableEditTagArgs.set("entity", entityToValue(root));
    undoableEditTagArgs.set("tag", std::string{"Undoable Root"});
    if (!tooling::CommandManager::get().execute(tooling::EditTagCommandId, context, undoableEditTagArgs).success ||
        !tooling::CommandManager::get().canUndo()) {
        std::cerr << "scene.edit_tag was not recorded as an undoable command.\n";
        return 1;
    }
    if (!tooling::CommandManager::get().undo(context) ||
        scene.getComponent<scene::TagComponent>(root).tag != "Edited Root") {
        std::cerr << "Undo did not restore scene.edit_tag.\n";
        return 1;
    }
    tooling::CommandManager::get().clearHistory();

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
