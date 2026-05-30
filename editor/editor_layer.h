#pragma once

#include "../LunaLite/core/layer.h"
#include "../LunaLite/scene/scene.h"
#include "editor_camera.h"
#include "hierarchy_panel.h"
#include "inspector_panel.h"

#include <filesystem>

namespace lunalite::editor {

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

    void createScene();
    void openScene();
    void saveScene();
    bool loadScene(const std::filesystem::path& scene_path);
    std::filesystem::path projectRelativePath(const std::filesystem::path& path) const;

    EditorCamera m_editor_camera;
    scene::Scene m_scene;
    scene::Entity m_selected_entity;
    HierarchyPanel m_hierarchy_panel;
    InspectorPanel m_inspector_panel;
    bool m_viewport_hovered{false};
    std::filesystem::path m_current_scene_path;
};

} // namespace lunalite::editor
