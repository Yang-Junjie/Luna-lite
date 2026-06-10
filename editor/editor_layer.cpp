#include "../LunaLite/asset/asset_manager.h"
#include "../LunaLite/core/application.h"
#include "../LunaLite/core/log.h"
#include "../LunaLite/imgui/imgui_renderer.h"
#include "../LunaLite/platform/common/file_dialogs.h"
#include "../LunaLite/project/project_manager.h"
#include "../LunaLite/renderer/debug_renderer.h"
#include "../LunaLite/renderer/interface/frustum.h"
#include "../LunaLite/renderer/interface/mesh.h"
#include "../LunaLite/scene/components.h"
#include "../LunaLite/scene/scene_renderer.h"
#include "../LunaLite/scene/scene_serializer.h"
#include "../LunaLiteTooling/commands/command_registry.h"
#include "../LunaLiteTooling/commands/scene_commands.h"
#include "../LunaLiteTooling/context/tool_context.h"
#include "editor_layer.h"

#include <cstdint>

#include <filesystem>
#include <glm/gtc/matrix_inverse.hpp>
#include <imgui.h>
#include <optional>
#include <type_traits>

namespace lunalite::editor {
namespace {
using EntityUnderlying = std::underlying_type_t<entt::entity>;

scene::Entity entityFromCommandValue(uint64_t value)
{
    return scene::Entity{static_cast<entt::entity>(static_cast<EntityUnderlying>(value))};
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

std::optional<renderer::interface::AABB> meshRendererWorldAABB(scene::Scene& scene, scene::Entity entity)
{
    if (!scene.isValidEntity(entity) || !scene.hasComponent<scene::MeshRendererComponent>(entity)) {
        return std::nullopt;
    }

    const auto& meshRenderer = scene.getComponent<scene::MeshRendererComponent>(entity);
    if (!meshRenderer.mesh.isValid()) {
        return std::nullopt;
    }

    const auto* mesh = asset::AssetManager::get().getAsset<renderer::interface::Mesh>(meshRenderer.mesh);
    if (mesh == nullptr) {
        return std::nullopt;
    }

    const auto localAabb = mesh->getLocalAABB(meshRenderer.submesh_start, meshRenderer.submesh_count);
    return localAabb.transformed(scene.getWorldTransform(entity));
}
} // namespace

EditorLayer::EditorLayer()
    : Layer("EditorLayer"),
      m_editor_setting_panel(m_editor_camera),
      m_hierarchy_panel(m_scene, m_selection),
      m_inspector_panel(m_scene, m_selection),
      m_scene_panel(m_scene),
      m_content_browser_panel(m_selection)
{}

void EditorLayer::onAttach() {}

void EditorLayer::onUpdate(core::Timestep dt)
{
    if (m_scene_state == SceneState::Play) {
        m_runtime_scene.onUpdateRuntime(dt);
        return;
    }

    m_scene.onUpdateEditor(dt);
    m_editor_camera.onUpdate(dt, m_viewport_hovered);
}

void EditorLayer::onRender()
{
    if (m_scene_state == SceneState::Play) {
        core::Application::get().getSceneRenderer().onRenderRuntime(m_runtime_scene);
        return;
    }

    core::Application::get().getSceneRenderer().onRenderEditor(m_scene, m_editor_camera);
    drawDebugOverlays();
}

void EditorLayer::onImGuiRender()
{
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    }
    drawMenuBar();
    m_hierarchy_panel.onImGuiRender();
    m_inspector_panel.onImGuiRender();
    m_scene_panel.onImGuiRender();
    m_material_editor_panel.onImGuiRender();
    m_editor_setting_panel.onImGuiRender();
    m_render_stats_panel.onImGuiRender();
    m_content_browser_panel.onImGuiRender();
    m_debug_panel.onImGuiRender();
    drawViewport();
}

void EditorLayer::drawMenuBar()
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("Project")) {
        if (ImGui::MenuItem("Open Project")) {
            openProject();
        }
        if (ImGui::MenuItem("Create Project")) {
            createProject();
        }
        if (ImGui::MenuItem("Save Project")) {
            saveProject();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene")) {
        if (ImGui::MenuItem("Open Scene")) {
            openScene();
        }
        if (ImGui::MenuItem("Create Scene")) {
            createScene();
        }
        if (ImGui::MenuItem("Save Scene")) {
            saveScene();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Runtime")) {
        if (ImGui::MenuItem("Play", nullptr, false, m_scene_state != SceneState::Play)) {
            startRuntime();
        }
        if (ImGui::MenuItem("Stop", nullptr, false, m_scene_state == SceneState::Play)) {
            stopRuntime();
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void EditorLayer::drawDebugOverlays()
{
    const auto& settings = m_debug_panel.overlaySettings();
    const auto activeCamera = makeEditorCameraSnapshot();
    if (m_debug_panel.consumeCaptureCullingFrustumRequest()) {
        m_frozen_culling_frustum = activeCamera;
        m_debug_panel.setFrozenCullingFrustumValid(true);
    }

    if (!settings.mesh_aabb && !settings.geometry_culling_aabb && !settings.selected_aabb &&
        !settings.show_culling_frustum) {
        return;
    }

    auto& debugRenderer = core::Application::get().getDebugRenderer();
    const DebugCameraSnapshot* cullingCamera = &activeCamera;
    if (settings.culling_source == DebugFrustumSource::FrozenEditorCamera && m_frozen_culling_frustum.valid) {
        cullingCamera = &m_frozen_culling_frustum;
    }
    const auto cullingFrustum = renderer::interface::Frustum::fromViewProjection(cullingCamera->view_projection);

    if (settings.show_culling_frustum) {
        const bool frozen =
            settings.culling_source == DebugFrustumSource::FrozenEditorCamera && m_frozen_culling_frustum.valid;
        const auto color = frozen ? glm::vec4{1.0f, 0.55f, 0.1f, 1.0f} : glm::vec4{0.1f, 0.85f, 1.0f, 1.0f};
        debugRenderer.drawFrustum(
            cullingCamera->inverse_view_projection, color, settings.depth_test, settings.culling_frustum_display_depth);
    }

    if (settings.mesh_aabb || settings.geometry_culling_aabb) {
        const auto meshView =
            m_scene.getRegistry().view<const scene::TransformComponent, const scene::MeshRendererComponent>();
        for (const auto entity : meshView) {
            const auto worldAabb = meshRendererWorldAABB(m_scene, scene::Entity{entity});
            if (!worldAabb) {
                continue;
            }

            auto color = glm::vec4{0.1f, 0.85f, 1.0f, 1.0f};
            if (settings.geometry_culling_aabb) {
                color = cullingFrustum.intersects(*worldAabb) ? glm::vec4{0.2f, 1.0f, 0.25f, 1.0f}
                                                              : glm::vec4{1.0f, 0.15f, 0.1f, 1.0f};
            }
            debugRenderer.drawAABB(*worldAabb, color, settings.depth_test);
        }
    }

    if (settings.selected_aabb) {
        const auto selectedEntity = m_selection.isEntity() ? m_selection.selectedEntity() : scene::Entity{};
        const auto selectedAabb = meshRendererWorldAABB(m_scene, selectedEntity);
        if (selectedAabb) {
            debugRenderer.drawAABB(*selectedAabb, glm::vec4{1.0f, 0.9f, 0.1f, 1.0f}, settings.depth_test);
        }
    }
}

EditorLayer::DebugCameraSnapshot EditorLayer::makeEditorCameraSnapshot() const
{
    const auto aspectRatio =
        m_viewport_height > 0 ? static_cast<float>(m_viewport_width) / static_cast<float>(m_viewport_height) : 1.0f;
    const auto view = m_editor_camera.getView();
    const auto projection = m_editor_camera.getProjection(aspectRatio);
    const auto viewProjection = projection * view;
    return DebugCameraSnapshot{
        .view = view,
        .projection = projection,
        .view_projection = viewProjection,
        .inverse_view_projection = glm::inverse(viewProjection),
        .valid = true,
    };
}

void EditorLayer::createProject()
{
    stopRuntime();

    const auto projectRoot = FileDialogs::selectDirectory({});
    if (projectRoot.empty()) {
        return;
    }

    project::ProjectInfo info;
    info.name = projectRoot.filename().string();
    info.assets_path = "Assets";

    if (!project::ProjectManager::instance().createProject(projectRoot, info)) {
        return;
    }

    asset::AssetManager::get().loadProjectAssets();
    m_scene.clear();
    m_selection.clear();
    m_current_scene_path.clear();
}

void EditorLayer::openProject()
{
    stopRuntime();

    const auto projectPath = FileDialogs::openFile("Luna Project\0*.lunaproj\0", {});
    if (projectPath.empty()) {
        return;
    }

    auto& projectManager = project::ProjectManager::instance();
    if (!projectManager.loadProject(projectPath)) {
        return;
    }

    asset::AssetManager::get().loadProjectAssets();
    m_scene.clear();
    m_selection.clear();
    m_current_scene_path.clear();

    const auto& projectInfo = projectManager.getProjectInfo();
    const auto projectRoot = projectManager.getProjectRootPath();
    if (projectInfo && projectRoot && !projectInfo->start_scene.empty()) {
        loadScene(*projectRoot / projectInfo->start_scene);
    }
}

void EditorLayer::saveProject()
{
    project::ProjectManager::instance().saveProject();
}

void EditorLayer::startRuntime()
{
    if (m_scene_state == SceneState::Play) {
        return;
    }

    m_runtime_scene.copyFrom(m_scene);
    m_runtime_scene.onRuntimeStart();
    m_scene_state = SceneState::Play;
}

void EditorLayer::stopRuntime()
{
    if (m_scene_state != SceneState::Play) {
        return;
    }

    m_runtime_scene.onRuntimeStop();
    m_runtime_scene.clear();
    m_scene_state = SceneState::Edit;
}

void EditorLayer::createScene()
{
    stopRuntime();

    auto scenePath = FileDialogs::saveFile("Luna Scene\0*.lunascene\0", {});
    if (scenePath.empty()) {
        return;
    }
    if (scenePath.extension().empty()) {
        scenePath.replace_extension(scene::SceneSerializer::FileExtension);
    }

    m_scene.clear();
    m_selection.clear();
    if (!scene::SceneSerializer::serialize(m_scene, scenePath)) {
        return;
    }

    m_current_scene_path = scenePath;

    auto& projectManager = project::ProjectManager::instance();
    const auto& projectInfo = projectManager.getProjectInfo();
    if (projectInfo) {
        auto info = *projectInfo;
        info.start_scene = projectRelativePath(scenePath);
        projectManager.setProjectInfo(info);
        projectManager.saveProject();
    }
}

void EditorLayer::openScene()
{
    const auto scenePath = FileDialogs::openFile("Luna Scene\0*.lunascene\0", {});
    if (scenePath.empty()) {
        return;
    }

    loadScene(scenePath);
}

void EditorLayer::saveScene()
{
    if (m_current_scene_path.empty()) {
        createScene();
        return;
    }

    scene::SceneSerializer::serialize(m_scene, m_current_scene_path);
}

bool EditorLayer::loadScene(const std::filesystem::path& scene_path)
{
    stopRuntime();

    if (!scene::SceneSerializer::deserialize(m_scene, scene_path)) {
        return false;
    }

    m_selection.clear();
    m_current_scene_path = scene_path;
    return true;
}

void EditorLayer::createEntityFromAsset(const AssetDragDropPayload& payload)
{
    if (!payload.handle.isValid()) {
        return;
    }

    tooling::ToolContext context;
    context.setScene(m_scene);

    tooling::CommandArgs args;
    args.set("source_asset", payload.handle);
    args.set("asset_type", static_cast<uint64_t>(payload.type));

    const auto result = tooling::CommandRegistry::get().execute(tooling::CreateEntityFromAssetCommandId, context, args);
    if (!result.success) {
        LUNA_CORE_ERROR("Failed to create entity from asset '{}': {}",
                        payload.handle.toString(),
                        result.message.empty() ? "unknown error" : result.message);
        return;
    }

    if (const auto entity = entityFromCommandResult(result)) {
        m_selection.selectEntity(*entity);
    }
}

std::filesystem::path EditorLayer::projectRelativePath(const std::filesystem::path& path) const
{
    const auto projectRoot = project::ProjectManager::instance().getProjectRootPath();
    if (!projectRoot) {
        return path;
    }

    const auto relativePath = path.lexically_relative(*projectRoot);
    return relativePath.empty() ? path : relativePath;
}

void EditorLayer::drawViewport()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Viewport");
    m_viewport_hovered = false;

    const auto& frame_image = core::Application::get().getFrameImage();
    const ImTextureID texture = core::Application::get().getImGuiRenderer().textureId(frame_image);
    const ImVec2 available = ImGui::GetContentRegionAvail();
    if (texture != ImTextureID_Invalid && frame_image.width > 0 && frame_image.height > 0 && available.x > 1.0f &&
        available.y > 1.0f) {
        m_viewport_width = static_cast<uint32_t>(available.x);
        m_viewport_height = static_cast<uint32_t>(available.y);
        core::Application::get().getSceneRenderer().setViewportSize(m_viewport_width, m_viewport_height);
        ImGui::Image(texture, available, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        m_viewport_hovered = ImGui::IsItemHovered();
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetDragDropPayloadName)) {
                if (payload->DataSize == sizeof(AssetDragDropPayload)) {
                    createEntityFromAsset(*static_cast<const AssetDragDropPayload*>(payload->Data));
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace lunalite::editor
