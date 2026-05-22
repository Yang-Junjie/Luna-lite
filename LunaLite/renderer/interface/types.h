#pragma once
#include <glm/glm.hpp>

namespace lunalite::renderer::interface {
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 color;
    // glm::vec3 tangent;
    // glm::vec3 bitangent;
};
} // namespace lunalite::renderer::interface
