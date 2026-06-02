#include "../core/log.h"
#include "debug_renderer.h"

namespace lunalite::renderer {

DebugRenderer::DebugRenderer(interface::Renderer& renderer)
    : m_renderer(&renderer)
{}

void DebugRenderer::setRenderer(interface::Renderer& renderer)
{
    m_renderer = &renderer;
}

void DebugRenderer::setViewProjection(const glm::mat4& view,
                                      const glm::mat4& projection,
                                      const glm::vec3& cameraPos,
                                      float exposure)
{
    LUNA_ASSERT(m_renderer, "DebugRenderer has no renderer.");
    m_renderer->setViewProjection(view, projection, cameraPos, exposure);
}

void DebugRenderer::renderLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
{
    LUNA_ASSERT(m_renderer, "DebugRenderer has no renderer.");
    m_renderer->renderLine(start, end, color);
}

} // namespace lunalite::renderer
