#pragma once

#include "../LunaLite/core/layer.h"
#include "../LunaLite/scene/scene.h"
#include "editor_camera.h"

namespace lunalite::editor {

class EditorLayer final : public core::Layer {
public:
    EditorLayer();

    void onAttach() override;
    void onUpdate(core::Timestep dt) override;
    void onRender() override;
    void onImGuiRender() override;

private:
    void drawViewport();

    EditorCamera m_editor_camera;
    scene::Scene m_scene;
    scene::Entity m_model_entity;
    bool m_viewport_hovered{false};
};

} // namespace lunalite::editor
