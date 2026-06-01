#pragma once
#include "interface/renderer.h"

#include <glm/glm.hpp>

namespace lunalite::renderer {

class DebugRenderer {
public:
    explicit DebugRenderer(interface::Renderer& renderer);

    void setRenderer(interface::Renderer& renderer);
    void setViewProjection(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos);
    void renderLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);

private:
    interface::Renderer* m_renderer{nullptr};
};

} // namespace lunalite::renderer
