#pragma once
#include "../LunaLite/core/layer.h"
#include "../LunaLite/scene/scene.h"

namespace lunalite::runtime {
class DemoLayer final : public core::Layer {
public:
    DemoLayer();

    void onAttach() override;
    void onRender() override;

private:
    scene::Scene m_scene;
};
} // namespace lunalite::runtime
