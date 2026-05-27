#pragma once

#include "../LunaLite/core/layer.h"
#include "../LunaLite/scene/scene.h"

namespace lunalite::editor {

class EditorLayer final : public core::Layer {
public:
    EditorLayer();

    void onAttach() override;
    void onUpdate(core::Timestep dt) override;
    void onRender() override;
    void onImGuiRender() override;

private:
    void drawDockspace();
    void drawViewport();

    scene::Scene m_scene;
    scene::Entity m_model_entity;
};

} // namespace lunalite::editor
