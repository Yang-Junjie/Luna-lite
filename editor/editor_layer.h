#pragma once

#include "../LunaLite/core/layer.h"
#include "../LunaLite/scene/scene.h"
#include "../LunaLiteTooling/context/selection_context.h"
#include "drag_drop.h"
#include "editor_camera.h"
#include "panels/content_browser_panel.h"
#include "panels/debug_panel.h"
#include "panels/editor_setting_panel.h"
#include "panels/hierarchy_panel.h"
#include "panels/inspector_panel.h"
#include "panels/material_editor_panel.h"
#include "panels/project_settings_panel.h"
#include "panels/render_stats_panel.h"
#include "panels/scene_panel.h"

#include <filesystem>
#include <glm/glm.hpp>

namespace lunalite::core {
class Event;
class KeyPressedEvent;
} // namespace lunalite::core

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
    void onEvent(core::Event& event) override;
    void onImGuiRender() override;

private:
    struct DebugCameraSnapshot {
        glm::mat4 view{1.0f};
        glm::mat4 projection{1.0f};
        glm::mat4 view_projection{1.0f};
        glm::mat4 inverse_view_projection{1.0f};
        bool valid{false};
    };

    void drawMenuBar();
    void drawViewport();
    void drawDebugOverlays();
    DebugCameraSnapshot makeEditorCameraSnapshot() const;
    void createProject();
    void openProject();
    void saveProject();
    void startRuntime();
    void stopRuntime();

    void createScene();
    void openScene();
    void saveScene();
    void restoreProjectScene();
    bool loadScene(const std::filesystem::path& scene_path);
    void persistEditorSceneCamera(bool force = false);

    void createEntityFromAsset(const drag_drop::AssetPayload& payload);
    void handleViewportAssetDrop(const drag_drop::AssetPayload& payload);
    bool loadSceneFromAsset(const drag_drop::AssetPayload& payload);
    bool onKeyPressedEvent(core::KeyPressedEvent& event);

    void createSelectedEntity(std::string name = {}, scene::Entity parent = {});
    void deleteSelectedEntity();
    void unparentSelectedEntity();

    bool undoSceneHistory();
    bool redoSceneHistory();
    void restoreSelectionAfterSceneHistory(const tooling::Selection& previous_selection);

    bool canModifyScene() const;

    std::filesystem::path projectRelativePath(const std::filesystem::path& path) const;

    EditorCamera m_editor_camera;
    EditorSettingPanel m_editor_setting_panel;

    scene::Scene m_scene;
    scene::Scene m_runtime_scene;

    tooling::SelectionContext m_selection;

    HierarchyPanel m_hierarchy_panel;
    InspectorPanel m_inspector_panel;
    ScenePanel m_scene_panel;
    MaterialEditorPanel m_material_editor_panel;
    std::filesystem::path m_current_scene_path;
    ProjectSettingsPanel m_project_settings_panel;
    RenderStatsPanel m_render_stats_panel;
    ContentBrowserPanel m_content_browser_panel;
    DebugPanel m_debug_panel;
    SceneState m_scene_state{SceneState::Edit};
    uint32_t m_viewport_width{1};
    uint32_t m_viewport_height{1};
    bool m_viewport_hovered{false};
    DebugCameraSnapshot m_frozen_culling_frustum;
};

} // namespace lunalite::editor
