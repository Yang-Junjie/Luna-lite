#pragma once
#include "interface/frame_render_data.h"

#include <glm/glm.hpp>

namespace lunalite::renderer {

class DebugRenderer {
public:
    DebugRenderer() = default;

    void beginFrame(interface::FrameRenderData& frame);
    void endFrame();
    void setViewProjection(const glm::mat4& view,
                           const glm::mat4& projection,
                           const glm::vec3& cameraPos,
                           float exposure);
    void drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color, bool depth_test = true);
    void drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color, bool depth_test = true);
    void drawAABB(const interface::AABB& aabb, const glm::vec4& color, bool depth_test = true);
    void drawOBB(const interface::AABB& local_aabb,
                 const glm::mat4& transform,
                 const glm::vec4& color,
                 bool depth_test = true);
    void drawFrustum(const glm::mat4& inverse_view_projection,
                     const glm::vec4& color,
                     bool depth_test = true,
                     float display_depth_scale = 1.0f);
    void drawTransformAxes(const glm::mat4& transform, float size = 1.0f, bool depth_test = true);

private:
    interface::FrameRenderData* m_frame{nullptr};
};

} // namespace lunalite::renderer
