#include "../core/log.h"
#include "debug_renderer.h"

#include <cmath>
#include <cstddef>

#include <algorithm>
#include <array>
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

void DebugRenderer::drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color, bool depth_test)
{
    LUNA_ASSERT(m_frame, "DebugRenderer has no frame render data.");
    m_frame->debug_lines.push_back(interface::LineDrawCommand{
        .start = start,
        .end = end,
        .color = color,
        .depth_test = depth_test,
    });
}

void DebugRenderer::drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color, bool depth_test)
{
    drawLine(start, end, glm::vec4{color, 1.0f}, depth_test);
}

void DebugRenderer::drawAABB(const interface::AABB& aabb, const glm::vec4& color, bool depth_test)
{
    if (!aabb.valid) {
        return;
    }

    const auto corners = aabb.corners();
    static constexpr uint32_t edges[][2] = {
        {0, 1},
        {1, 3},
        {3, 2},
        {2, 0},
        {4, 5},
        {5, 7},
        {7, 6},
        {6, 4},
        {0, 4},
        {1, 5},
        {2, 6},
        {3, 7},
    };

    for (const auto& edge : edges) {
        drawLine(corners[edge[0]], corners[edge[1]], color, depth_test);
    }
}

void DebugRenderer::drawOBB(const interface::AABB& local_aabb,
                            const glm::mat4& transform,
                            const glm::vec4& color,
                            bool depth_test)
{
    if (!local_aabb.valid) {
        return;
    }

    const auto corners = local_aabb.corners();
    std::array<glm::vec3, 8> transformedCorners{};
    for (size_t cornerIndex = 0; cornerIndex < corners.size(); ++cornerIndex) {
        transformedCorners[cornerIndex] = glm::vec3{transform * glm::vec4{corners[cornerIndex], 1.0f}};
    }

    static constexpr uint32_t edges[][2] = {
        {0, 1},
        {1, 3},
        {3, 2},
        {2, 0},
        {4, 5},
        {5, 7},
        {7, 6},
        {6, 4},
        {0, 4},
        {1, 5},
        {2, 6},
        {3, 7},
    };

    for (const auto& edge : edges) {
        drawLine(transformedCorners[edge[0]], transformedCorners[edge[1]], color, depth_test);
    }
}

void DebugRenderer::drawFrustum(const glm::mat4& inverse_view_projection,
                                const glm::vec4& color,
                                bool depth_test,
                                float display_depth_scale)
{
    static const std::array<glm::vec3, 8> ndcCorners = {
        glm::vec3{-1.0f, -1.0f, -1.0f},
        glm::vec3{1.0f, -1.0f, -1.0f},
        glm::vec3{-1.0f, 1.0f, -1.0f},
        glm::vec3{1.0f, 1.0f, -1.0f},
        glm::vec3{-1.0f, -1.0f, 1.0f},
        glm::vec3{1.0f, -1.0f, 1.0f},
        glm::vec3{-1.0f, 1.0f, 1.0f},
        glm::vec3{1.0f, 1.0f, 1.0f},
    };
    static constexpr uint32_t edges[][2] = {
        {0, 1},
        {1, 3},
        {3, 2},
        {2, 0},
        {4, 5},
        {5, 7},
        {7, 6},
        {6, 4},
        {0, 4},
        {1, 5},
        {2, 6},
        {3, 7},
    };

    std::array<glm::vec3, 8> worldCorners{};
    for (size_t cornerIndex = 0; cornerIndex < ndcCorners.size(); ++cornerIndex) {
        const auto world = inverse_view_projection * glm::vec4{ndcCorners[cornerIndex], 1.0f};
        if (std::abs(world.w) <= 0.000001f) {
            return;
        }
        worldCorners[cornerIndex] = glm::vec3{world} / world.w;
    }

    const float depthScale = std::clamp(display_depth_scale, 0.0f, 1.0f);
    for (size_t cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
        const size_t farCornerIndex = cornerIndex + 4;
        worldCorners[farCornerIndex] =
            worldCorners[cornerIndex] + (worldCorners[farCornerIndex] - worldCorners[cornerIndex]) * depthScale;
    }

    for (const auto& edge : edges) {
        drawLine(worldCorners[edge[0]], worldCorners[edge[1]], color, depth_test);
    }
}

void DebugRenderer::drawTransformAxes(const glm::mat4& transform, float size, bool depth_test)
{
    const auto origin = glm::vec3{transform * glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}};
    const auto xAxis = glm::vec3{transform * glm::vec4{size, 0.0f, 0.0f, 1.0f}};
    const auto yAxis = glm::vec3{transform * glm::vec4{0.0f, size, 0.0f, 1.0f}};
    const auto zAxis = glm::vec3{transform * glm::vec4{0.0f, 0.0f, size, 1.0f}};

    drawLine(origin, xAxis, glm::vec4{1.0f, 0.1f, 0.1f, 1.0f}, depth_test);
    drawLine(origin, yAxis, glm::vec4{0.1f, 1.0f, 0.1f, 1.0f}, depth_test);
    drawLine(origin, zAxis, glm::vec4{0.1f, 0.3f, 1.0f, 1.0f}, depth_test);
}

} // namespace lunalite::renderer
