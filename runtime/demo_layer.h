#pragma once
#include "../LunaLite/core/layer.h"
#include "../LunaLite/scene/scene.h"

namespace lunalite::runtime {
class DemoLayer final : public core::Layer {
public:
    DemoLayer();

    void onAttach() override;
    void onUpdate(core::Timestep dt) override;
    void onRender() override;

private:
    scene::Scene m_scene;
    scene::Entity m_model_entity;
};
} // namespace lunalite::runtime
