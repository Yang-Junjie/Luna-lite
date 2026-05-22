#pragma once
#include "../asset/asset_database.h"
#include "../renderer/interface/renderer.h"
#include "scene.h"

namespace lunalite::scene {
class SceneRenderer {
public:
    SceneRenderer(renderer::interface::Renderer& renderer, asset::AssetDatabase& assets);
    ~SceneRenderer() = default;
    void render(const Scene& scene);

private:
    renderer::interface::Renderer& m_renderer;
    asset::AssetDatabase& m_assets;
};
} // namespace lunalite::scene
