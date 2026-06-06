#pragma once

#include <array>
#include <glm/glm.hpp>

namespace lunalite::renderer::interface {
struct AABB {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
    bool valid{false};

    void include(const glm::vec3& point)
    {
        if (!valid) {
            min = point;
            max = point;
            valid = true;
            return;
        }

        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    void include(const AABB& other)
    {
        if (!other.valid) {
            return;
        }

        include(other.min);
        include(other.max);
    }

    std::array<glm::vec3, 8> corners() const
    {
        return {
            glm::vec3{min.x, min.y, min.z},
            glm::vec3{max.x, min.y, min.z},
            glm::vec3{min.x, max.y, min.z},
            glm::vec3{max.x, max.y, min.z},
            glm::vec3{min.x, min.y, max.z},
            glm::vec3{max.x, min.y, max.z},
            glm::vec3{min.x, max.y, max.z},
            glm::vec3{max.x, max.y, max.z},
        };
    }

    AABB transformed(const glm::mat4& transform) const
    {
        AABB result;
        if (!valid) {
            return result;
        }

        for (const auto& corner : corners()) {
            result.include(glm::vec3{transform * glm::vec4{corner, 1.0f}});
        }
        return result;
    }
};
} // namespace lunalite::renderer::interface
