#pragma once
#include "scene.h"

namespace lunalite::core {
class Application;
}

namespace lunalite::renderer::interface {
class Renderer;
}

namespace lunalite::scene {
class SceneRenderer {
public:
    ~SceneRenderer() = default;
    void render(const Scene& scene);

private:
    // 只有Applicaytion可以创建SceneRenderer使用beginFrame和endFrame
    friend class core::Application;

    explicit SceneRenderer(renderer::interface::Renderer& renderer);

    void beginFrame();
    void endFrame();

private:
    renderer::interface::Renderer& m_renderer;
};
} // namespace lunalite::scene
