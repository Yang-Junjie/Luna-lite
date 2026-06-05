#pragma once
#include "../renderer/interface/camera.h"
#include "../renderer/interface/frame_render_data.h"
#include "scene.h"

#include <cstdint>

#include <glm/glm.hpp>

namespace lunalite::core {
class Application;
}

namespace lunalite::scene {
class SceneRenderer {
public:
    ~SceneRenderer() = default;
    void setViewportSize(uint32_t width, uint32_t height);
    void onRenderRuntime(const Scene& scene);
    void onRenderEditor(const Scene& scene, const renderer::interface::Camera& camera);

private:
    // 只有Application可以创建SceneRenderer使用beginFrame和endFrame
    friend class core::Application;

    SceneRenderer();

    void beginFrame(renderer::interface::FrameRenderData& frame);
    void endFrame();
    void renderScene(const Scene& scene,
                     const glm::mat4& view,
                     const glm::mat4& projection,
                     const glm::vec3& cameraPosition,
                     float exposure,
                     renderer::interface::FrameRenderData& frame);

private:
    renderer::interface::FrameRenderData* m_frame_data{nullptr};
    uint32_t m_viewport_width{1};
    uint32_t m_viewport_height{1};
};
} // namespace lunalite::scene
