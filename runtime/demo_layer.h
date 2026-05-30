#pragma once
#include "../LunaLite/core/layer.h"
#include "../LunaLite/scene/scene.h"

#include <filesystem>

namespace lunalite::runtime {
class DemoLayer final : public core::Layer {
public:
    explicit DemoLayer(std::filesystem::path project_search_path);

    void onAttach() override;
    void onUpdate(core::Timestep dt) override;
    void onRender() override;
    void onImGuiRender() override;

private:
    std::filesystem::path m_project_search_path;
    scene::Scene m_scene;
};
} // namespace lunalite::runtime
