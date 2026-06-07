#pragma once
#include "aabb.h"

#include <array>
#include <glm/glm.hpp>

namespace lunalite::renderer::interface {

struct FrustumPlane {
    glm::vec3 normal{0.0f};
    float distance{0.0f};

    float signedDistance(const glm::vec3& point) const
    {
        return glm::dot(normal, point) + distance;
    }
};

class Frustum {
public:
    static Frustum fromViewProjection(const glm::mat4& view_projection)
    {
        const auto row = [&view_projection](int index) {
            return glm::vec4{
                view_projection[0][index],
                view_projection[1][index],
                view_projection[2][index],
                view_projection[3][index],
            };
        };

        Frustum frustum;
        const auto row0 = row(0);
        const auto row1 = row(1);
        const auto row2 = row(2);
        const auto row3 = row(3);
        frustum.m_planes = {
            makePlane(row3 + row0),
            makePlane(row3 - row0),
            makePlane(row3 + row1),
            makePlane(row3 - row1),
            makePlane(row3 + row2),
            makePlane(row3 - row2),
        };
        return frustum;
    }

    bool intersects(const AABB& aabb) const
    {
        if (!aabb.valid) {
            return true;
        }

        for (const auto& plane : m_planes) {
            glm::vec3 positiveVertex = aabb.min;
            if (plane.normal.x >= 0.0f) {
                positiveVertex.x = aabb.max.x;
            }
            if (plane.normal.y >= 0.0f) {
                positiveVertex.y = aabb.max.y;
            }
            if (plane.normal.z >= 0.0f) {
                positiveVertex.z = aabb.max.z;
            }

            if (plane.signedDistance(positiveVertex) < 0.0f) {
                return false;
            }
        }

        return true;
    }

private:
    static FrustumPlane makePlane(const glm::vec4& equation)
    {
        const auto normal = glm::vec3{equation};
        const auto length = glm::length(normal);
        if (length <= 0.000001f) {
            return {};
        }

        return FrustumPlane{
            .normal = normal / length,
            .distance = equation.w / length,
        };
    }

    std::array<FrustumPlane, 6> m_planes{};
};

} // namespace lunalite::renderer::interface
