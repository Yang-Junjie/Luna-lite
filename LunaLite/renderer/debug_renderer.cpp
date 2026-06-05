#include "../core/log.h"
#include "debug_renderer.h"

#include <glm/gtc/matrix_inverse.hpp>

namespace lunalite::renderer {

void DebugRenderer::beginFrame(interface::FrameRenderData& frame)
{
    m_frame = &frame;
}

void DebugRenderer::endFrame()
{
    m_frame = nullptr;
}

void DebugRenderer::setViewProjection(const glm::mat4& view,
                                      const glm::mat4& projection,
                                      const glm::vec3& cameraPos,
                                      float exposure)
{
    LUNA_ASSERT(m_frame, "DebugRenderer has no frame render data.");
    m_frame->camera.view = view;
    m_frame->camera.projection = projection;
    m_frame->camera.view_projection = projection * view;
    m_frame->camera.inverse_view_projection = glm::inverse(m_frame->camera.view_projection);
    m_frame->camera.position = cameraPos;
    m_frame->camera.exposure = exposure;
}

void DebugRenderer::renderLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
{
    LUNA_ASSERT(m_frame, "DebugRenderer has no frame render data.");
    m_frame->debug_lines.push_back(
        interface::LineDrawCommand{.start = start, .end = end, .color = glm::vec4{color, 1.0f}});
}

} // namespace lunalite::renderer
