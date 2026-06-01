#pragma once
#include "../LunaLite/core/layer.h"
#include "../LunaLite/scene/scene.h"

namespace lunalite::test_app {
class TestAppLayer final : public core::Layer {
public:
    TestAppLayer();

    void onAttach() override;
    void onRender() override;

private:
    scene::Scene m_scene;
};
} // namespace lunalite::test_app
