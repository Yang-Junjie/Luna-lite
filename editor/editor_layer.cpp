#include "../LunaLite/asset/asset_manager.h"
#include "../LunaLite/core/application.h"
#include "../LunaLite/imgui/imgui_renderer.h"
#include "../LunaLite/platform/common/file_dialogs.h"
#include "../LunaLite/project/project_manager.h"
#include "../LunaLite/scene/components.h"
#include "../LunaLite/scene/scene_renderer.h"
#include "../LunaLite/scene/scene_serializer.h"
#include "editor_layer.h"

#include <cstdint>

#include <imgui.h>

namespace lunalite::editor {

EditorLayer::EditorLayer()
    : Layer("EditorLayer"),
      m_editor_setting_panel(m_editor_camera),
      m_hierarchy_panel(m_scene, m_selected_entity),
      m_inspector_panel(m_scene, m_selected_entity)
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
}

void EditorLayer::onImGuiRender()
{
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    }
    drawMenuBar();
    m_hierarchy_panel.onImGuiRender();
    m_inspector_panel.onImGuiRender();
    m_material_editor_panel.onImGuiRender();
    m_editor_setting_panel.onImGuiRender();
    m_content_browser_panel.onImGuiRender();
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
    m_selected_entity = scene::Entity{};
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
    m_selected_entity = scene::Entity{};
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
    m_selected_entity = scene::Entity{};
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

    m_selected_entity = scene::Entity{};
    m_current_scene_path = scene_path;
    return true;
}

void EditorLayer::createEntityFromAsset(const AssetDragDropPayload& payload)
{
    const auto handle = payload.handle;
    if (!handle.isValid()) {
        return;
    }

    if (payload.type == asset::AssetType::Model) {
        auto entity = m_scene.createEntity();
        auto& model = m_scene.addComponent<scene::ModelComponent>(entity);
        model.model = handle;

        if (const auto* metadata = asset::AssetManager::get().getMetadata(handle)) {
            auto& tag = m_scene.getComponent<scene::TagComponent>(entity);
            tag.tag = metadata->Name.empty() ? metadata->FilePath.stem().string() : metadata->Name;
        }

        m_selected_entity = entity;
        return;
    }

    if (payload.type == asset::AssetType::Script) {
        auto entity = m_scene.createEntity();
        auto& script = m_scene.addComponent<scene::ScriptComponent>(entity);
        script.scripts.push_back({handle, true});

        if (const auto* metadata = asset::AssetManager::get().getMetadata(handle)) {
            auto& tag = m_scene.getComponent<scene::TagComponent>(entity);
            tag.tag = metadata->Name.empty() ? metadata->FilePath.stem().string() : metadata->Name;
        }

        m_selected_entity = entity;
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
        core::Application::get().getSceneRenderer().setViewportSize(static_cast<uint32_t>(available.x),
                                                                    static_cast<uint32_t>(available.y));
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
