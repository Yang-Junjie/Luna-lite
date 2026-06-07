#pragma once
#include "frame_render_data.h"

#include <array>
#include <cstddef>

#include <glm/glm.hpp>

namespace lunalite::renderer::interface {

inline std::array<glm::vec3, 4> spriteLocalCorners(const SpriteDrawCommand& sprite)
{
    const float left = -sprite.pivot.x * sprite.size.x;
    const float right = (1.0f - sprite.pivot.x) * sprite.size.x;
    const float bottom = -sprite.pivot.y * sprite.size.y;
    const float top = (1.0f - sprite.pivot.y) * sprite.size.y;

    return {
        glm::vec3{left, bottom, 0.0f},
        glm::vec3{right, bottom, 0.0f},
        glm::vec3{right, top, 0.0f},
        glm::vec3{left, top, 0.0f},
    };
}

inline glm::vec3 transformSpritePoint(const glm::mat4& transform, const glm::vec3& point)
{
    return glm::vec3{transform * glm::vec4{point, 1.0f}};
}

inline bool spriteIntersectsClipVolume(const SpriteDrawCommand& sprite, const glm::mat4& view_projection)
{
    const auto corners = spriteLocalCorners(sprite);
    std::array<glm::vec4, 4> clipCorners{};
    for (size_t i = 0; i < corners.size(); ++i) {
        clipCorners[i] = view_projection * sprite.transform * glm::vec4{corners[i], 1.0f};
    }

    const auto outsideLeft = [](const glm::vec4& corner) { return corner.x < -corner.w; };
    const auto outsideRight = [](const glm::vec4& corner) { return corner.x > corner.w; };
    const auto outsideBottom = [](const glm::vec4& corner) { return corner.y < -corner.w; };
    const auto outsideTop = [](const glm::vec4& corner) { return corner.y > corner.w; };
    const auto outsideNear = [](const glm::vec4& corner) { return corner.z < -corner.w; };
    const auto outsideFar = [](const glm::vec4& corner) { return corner.z > corner.w; };

    const auto allOutside = [&clipCorners](auto predicate) {
        return predicate(clipCorners[0]) && predicate(clipCorners[1]) && predicate(clipCorners[2]) &&
               predicate(clipCorners[3]);
    };

    return !allOutside(outsideLeft) && !allOutside(outsideRight) && !allOutside(outsideBottom) &&
           !allOutside(outsideTop) && !allOutside(outsideNear) && !allOutside(outsideFar);
}

} // namespace lunalite::renderer::interface
