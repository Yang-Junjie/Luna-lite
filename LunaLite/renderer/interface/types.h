#pragma once
#include <glm/glm.hpp>

namespace lunalite::renderer::interface {
struct Vertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f};
    glm::vec3 color{1.0f};
    // glm::vec3 tangent;
    // glm::vec3 bitangent;
};
} // namespace lunalite::renderer::interface
