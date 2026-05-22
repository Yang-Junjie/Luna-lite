#pragma once
#include "../renderer/interface/renderer.h"
#include "scene.h"

namespace lunalite::scene {
class SceneRenderer {
public:
    explicit SceneRenderer(renderer::interface::Renderer& renderer);
    ~SceneRenderer() = default;
    void render(const Scene& scene);

private:
    renderer::interface::Renderer& m_renderer;
};
} // namespace lunalite::scene
