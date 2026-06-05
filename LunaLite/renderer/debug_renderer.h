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
    void renderLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);

private:
    interface::FrameRenderData* m_frame{nullptr};
};

} // namespace lunalite::renderer
