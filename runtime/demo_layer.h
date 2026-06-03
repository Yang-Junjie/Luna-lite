#pragma once
#include "../LunaLite/core/layer.h"
#include "../LunaLite/scene/scene.h"

#include <filesystem>

namespace lunalite::runtime {
class DemoLayer final : public core::Layer {
public:
    explicit DemoLayer(std::filesystem::path project_search_path);

    void onAttach() override;
    void onDetach() override;
    void onUpdate(core::Timestep dt) override;
    void onRender() override;

private:
    void stopRuntime();

    std::filesystem::path m_project_search_path;
    scene::Scene m_scene;
    bool m_scene_loaded{false};
    bool m_runtime_started{false};
};
} // namespace lunalite::runtime
