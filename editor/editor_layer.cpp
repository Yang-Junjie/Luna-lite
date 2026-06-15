#include "../LunaLite/asset/asset_manager.h"
#include "../LunaLite/core/application.h"
#include "../LunaLite/core/event.h"
#include "../LunaLite/core/input.h"
#include "../LunaLite/core/key_codes.h"
#include "../LunaLite/core/key_event.h"
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
#include "../LunaLiteTooling/commands/command_manager.h"
#include "../LunaLiteTooling/context/tool_context.h"
#include "editor_actions.h"
#include "editor_layer.h"
#include "editor_scene_metadata.h"

#include <filesystem>
#include <glm/gtc/matrix_inverse.hpp>
#include <imgui.h>
#include <optional>
#include <utility>

namespace lunalite::editor {
namespace {
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
      m_project_settings_panel(m_current_scene_path),
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

void EditorLayer::onEvent(core::Event& event)
{
    core::EventDispatcher dispatcher(event);
    dispatcher.dispatch<core::KeyPressedEvent>([this](core::KeyPressedEvent& keyEvent) {
        return onKeyPressedEvent(keyEvent);
    });
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
    m_project_settings_panel.onImGuiRender();
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
        if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {
            openScene();
        }
        if (ImGui::MenuItem("Create Scene", "Ctrl+N")) {
            createScene();
        }
        if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
            saveScene();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        const bool can_modify_scene = canModifyScene();
        const bool canUndo = can_modify_scene && tooling::CommandManager::get().canUndo();
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo)) {
            undoSceneHistory();
        }
        const bool canRedo = can_modify_scene && tooling::CommandManager::get().canRedo();
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo)) {
            redoSceneHistory();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Create Entity", "Ctrl+Shift+N", false, can_modify_scene)) {
            createSelectedEntity();
        }
        const bool canDeleteSelection =
            can_modify_scene && m_selection.isEntity() && m_scene.isValidEntity(m_selection.selectedEntity());
        if (ImGui::MenuItem("Delete Selected", "Del", false, canDeleteSelection)) {
            deleteSelectedEntity();
        }
        const bool canUnparentSelection = can_modify_scene && m_selection.isEntity() &&
                                          m_scene.isValidEntity(m_selection.selectedEntity()) &&
                                          m_scene.getParent(m_selection.selectedEntity());
        if (ImGui::MenuItem("Unparent Selected", nullptr, false, canUnparentSelection)) {
            unparentSelectedEntity();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Runtime")) {
        if (ImGui::MenuItem("Play", "F5", false, m_scene_state != SceneState::Play)) {
            startRuntime();
        }
        if (ImGui::MenuItem("Stop", "F5", false, m_scene_state == SceneState::Play)) {
            stopRuntime();
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

bool EditorLayer::onKeyPressedEvent(core::KeyPressedEvent& event)
{
    if (event.isRepeat()) {
        return false;
    }

    const bool controlDown =
        core::Input::isKeyPressed(core::KeyCode::LeftControl) || core::Input::isKeyPressed(core::KeyCode::RightControl);
    const bool shiftDown =
        core::Input::isKeyPressed(core::KeyCode::LeftShift) || core::Input::isKeyPressed(core::KeyCode::RightShift);
    const bool wantsTextInput = ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput;

    switch (event.getKeyCode()) {
        case core::KeyCode::O:
            if (controlDown && !shiftDown) {
                openScene();
                return true;
            }
            break;
        case core::KeyCode::N:
            if (controlDown && !shiftDown) {
                createScene();
                return true;
            }
            if (controlDown && shiftDown) {
                if (!wantsTextInput && canModifyScene()) {
                    createSelectedEntity();
                    return true;
                }
            }
            break;
        case core::KeyCode::S:
            if (controlDown && !shiftDown) {
                saveScene();
                return true;
            }
            break;
        case core::KeyCode::Z:
            if (controlDown && !shiftDown && !wantsTextInput && canModifyScene()) {
                if (undoSceneHistory()) {
                    return true;
                }
            }
            if (controlDown && shiftDown && !wantsTextInput && canModifyScene()) {
                if (redoSceneHistory()) {
                    return true;
                }
            }
            break;
        case core::KeyCode::Y:
            if (controlDown && !shiftDown && !wantsTextInput && canModifyScene()) {
                if (redoSceneHistory()) {
                    return true;
                }
            }
            break;
        case core::KeyCode::F5:
            if (!controlDown && !shiftDown) {
                if (m_scene_state == SceneState::Play) {
                    stopRuntime();
                } else {
                    startRuntime();
                }
                return true;
            }
            break;
        case core::KeyCode::Delete:
            if (!wantsTextInput && canModifyScene()) {
                deleteSelectedEntity();
                return true;
            }
            break;
        default:
            break;
    }

    return false;
}

void EditorLayer::createSelectedEntity(std::string name, scene::Entity parent)
{
    if (!canModifyScene()) {
        return;
    }

    if (auto entity = actions::createEntity(m_scene, std::move(name), parent)) {
        m_selection.selectEntity(*entity);
    }
}

void EditorLayer::deleteSelectedEntity()
{
    if (!canModifyScene() || !m_selection.isEntity()) {
        return;
    }

    const auto selectedEntity = m_selection.selectedEntity();
    if (!m_scene.isValidEntity(selectedEntity)) {
        m_selection.clear();
        return;
    }

    actions::deleteEntity(m_scene, selectedEntity);
    if (m_selection.isEntity() && !m_scene.isValidEntity(m_selection.selectedEntity())) {
        m_selection.clear();
    }
}

void EditorLayer::unparentSelectedEntity()
{
    if (!canModifyScene() || !m_selection.isEntity()) {
        return;
    }

    const auto selectedEntity = m_selection.selectedEntity();
    if (!m_scene.isValidEntity(selectedEntity) || !m_scene.getParent(selectedEntity)) {
        return;
    }

    if (actions::clearParent(m_scene, selectedEntity, true)) {
        m_selection.selectEntity(selectedEntity);
    }
}

bool EditorLayer::undoSceneHistory()
{
    if (!canModifyScene()) {
        return false;
    }

    const auto previousSelection = m_selection.current();
    tooling::ToolContext context;
    context.setScene(m_scene);
    if (!tooling::CommandManager::get().undo(context)) {
        return false;
    }

    restoreSelectionAfterSceneHistory(previousSelection);
    return true;
}

bool EditorLayer::redoSceneHistory()
{
    if (!canModifyScene()) {
        return false;
    }

    const auto previousSelection = m_selection.current();
    tooling::ToolContext context;
    context.setScene(m_scene);
    if (!tooling::CommandManager::get().redo(context)) {
        return false;
    }

    restoreSelectionAfterSceneHistory(previousSelection);
    return true;
}

void EditorLayer::restoreSelectionAfterSceneHistory(const tooling::Selection& previous_selection)
{
    switch (previous_selection.kind) {
        case tooling::SelectionKind::Entity:
            if (m_scene.isValidEntity(previous_selection.entity)) {
                m_selection.selectEntity(previous_selection.entity);
            } else {
                m_selection.clear();
            }
            break;
        case tooling::SelectionKind::Asset:
            m_selection.selectAsset(previous_selection.asset);
            break;
        case tooling::SelectionKind::Folder:
            m_selection.selectFolder(previous_selection.path);
            break;
        case tooling::SelectionKind::Project:
            m_selection.selectProject();
            break;
        case tooling::SelectionKind::None:
        default:
            m_selection.clear();
            break;
    }
}

bool EditorLayer::canModifyScene() const
{
    return m_scene_state != SceneState::Play;
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

    const auto result = actions::createProject(projectRoot, info.name);
    if (!result.success) {
        LUNA_CORE_ERROR("Failed to create project command: {}",
                        result.message.empty() ? "unknown error" : result.message);
        return;
    }

    m_scene.clear();
    tooling::CommandManager::get().clearHistory();
    m_selection.clear();
    m_current_scene_path.clear();
    m_editor_camera.resetSceneState();
}

void EditorLayer::openProject()
{
    stopRuntime();

    const auto projectPath = FileDialogs::openFile("Luna Project\0*.lunaproj\0", {});
    if (projectPath.empty()) {
        return;
    }

    const auto result = actions::openProject(projectPath);
    if (!result.success) {
        LUNA_CORE_ERROR("Failed to open project command: {}",
                        result.message.empty() ? "unknown error" : result.message);
        return;
    }

    m_scene.clear();
    tooling::CommandManager::get().clearHistory();
    m_selection.clear();
    m_current_scene_path.clear();
    m_editor_camera.resetSceneState();

    restoreProjectScene();
}

void EditorLayer::saveProject()
{
    const auto result = actions::saveProject();
    if (!result.success) {
        LUNA_CORE_ERROR("Failed to save project command: {}",
                        result.message.empty() ? "unknown error" : result.message);
    }
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

    const auto result = actions::createSceneFile(m_scene, scenePath);
    if (!result.success) {
        LUNA_CORE_ERROR("Failed to create scene command: {}",
                        result.message.empty() ? "unknown error" : result.message);
        return;
    }

    tooling::CommandManager::get().clearHistory();
    m_selection.clear();
    if (const auto* savedPath = result.get<std::filesystem::path>("scene_path")) {
        m_current_scene_path = *savedPath;
    } else {
        m_current_scene_path = scenePath;
    }
    persistEditorSceneCamera(true);
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

    const auto result = actions::saveSceneFile(m_scene, m_current_scene_path);
    if (!result.success) {
        LUNA_CORE_ERROR("Failed to save scene command: {}", result.message.empty() ? "unknown error" : result.message);
        return;
    }

    persistEditorSceneCamera(true);
}

void EditorLayer::restoreProjectScene()
{
    const auto& projectInfo = project::ProjectManager::instance().getProjectInfo();
    const auto projectRoot = project::ProjectManager::instance().getProjectRootPath();
    if (!projectInfo || !projectRoot) {
        return;
    }

    if (!projectInfo->last_open_scene.empty() && loadScene(*projectRoot / projectInfo->last_open_scene)) {
        return;
    }

    if (!projectInfo->start_scene.empty()) {
        loadScene(*projectRoot / projectInfo->start_scene);
    }
}

bool EditorLayer::loadScene(const std::filesystem::path& scene_path)
{
    stopRuntime();
    tooling::CommandManager::get().clearHistory();

    const auto result = actions::openSceneFile(m_scene, scene_path);
    if (!result.success) {
        LUNA_CORE_ERROR("Failed to open scene command: {}", result.message.empty() ? "unknown error" : result.message);
        return false;
    }

    m_selection.clear();
    if (const auto* openedPath = result.get<std::filesystem::path>("scene_path")) {
        m_current_scene_path = *openedPath;
    } else {
        m_current_scene_path = scene_path;
    }
    loadEditorSceneCamera(m_current_scene_path, m_editor_camera);
    return true;
}

void EditorLayer::persistEditorSceneCamera(bool force)
{
    if (m_current_scene_path.empty()) {
        return;
    }

    if (!force && !m_editor_camera.hasSceneStateDirty()) {
        return;
    }

    if (saveEditorSceneCamera(m_current_scene_path, m_editor_camera)) {
        m_editor_camera.clearSceneStateDirty();
    }
}

void EditorLayer::createEntityFromAsset(const AssetDragDropPayload& payload)
{
    if (!payload.handle.isValid()) {
        return;
    }

    if (const auto entity = actions::createEntityFromAsset(m_scene, payload.handle, payload.type)) {
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
