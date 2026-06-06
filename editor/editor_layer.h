#pragma once

#include "../LunaLite/core/layer.h"
#include "../LunaLite/scene/scene.h"
#include "editor_camera.h"
#include "panels/content_browser_panel.h"
#include "panels/editor_setting_panel.h"
#include "panels/hierarchy_panel.h"
#include "panels/inspector_panel.h"
#include "panels/material_editor_panel.h"
#include "panels/scene_panel.h"

#include <filesystem>

namespace lunalite::editor {

enum class SceneState {
    Edit,
    Play
};

class EditorLayer final : public core::Layer {
public:
    EditorLayer();

    void onAttach() override;
    void onUpdate(core::Timestep dt) override;
    void onRender() override;
    void onImGuiRender() override;

private:
    void drawMenuBar();
    void drawViewport();
    void createProject();
    void openProject();
    void saveProject();
    void startRuntime();
    void stopRuntime();

    void createScene();
    void openScene();
    void saveScene();
    bool loadScene(const std::filesystem::path& scene_path);
    void createEntityFromAsset(const AssetDragDropPayload& payload);
    std::filesystem::path projectRelativePath(const std::filesystem::path& path) const;

    EditorCamera m_editor_camera;
    EditorSettingPanel m_editor_setting_panel;

    scene::Scene m_scene;
    scene::Scene m_runtime_scene;

    scene::Entity m_selected_entity;

    HierarchyPanel m_hierarchy_panel;
    InspectorPanel m_inspector_panel;
    ScenePanel m_scene_panel;
    MaterialEditorPanel m_material_editor_panel;
    ContentBrowserPanel m_content_browser_panel;
    SceneState m_scene_state{SceneState::Edit};
    bool m_viewport_hovered{false};
    std::filesystem::path m_current_scene_path;
};

} // namespace lunalite::editor
