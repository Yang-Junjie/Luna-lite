#include "../LunaLite/renderer/debug_renderer.h"

#include <glm/glm.hpp>
#include <iostream>

int main()
{
    using namespace lunalite;

    renderer::interface::FrameRenderData frame;
    renderer::DebugRenderer debugRenderer;
    debugRenderer.beginFrame(frame);

    debugRenderer.drawLine({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, glm::vec4{1.0f}, false);
    if (frame.debug_lines.size() != 1 || frame.debug_lines.front().depth_test) {
        std::cerr << "Debug line command was not recorded correctly.\n";
        return 1;
    }

    frame.debug_lines.clear();
    renderer::interface::AABB invalidAabb;
    debugRenderer.drawAABB(invalidAabb, glm::vec4{1.0f});
    if (!frame.debug_lines.empty()) {
        std::cerr << "Invalid AABB should not emit debug lines.\n";
        return 1;
    }

    renderer::interface::AABB aabb;
    aabb.include({-1.0f, -2.0f, -3.0f});
    aabb.include({1.0f, 2.0f, 3.0f});
    debugRenderer.drawAABB(aabb, glm::vec4{0.2f, 0.3f, 0.4f, 1.0f});
    if (frame.debug_lines.size() != 12) {
        std::cerr << "AABB should emit 12 debug lines.\n";
        return 1;
    }
    for (const auto& line : frame.debug_lines) {
        if (line.color != glm::vec4{0.2f, 0.3f, 0.4f, 1.0f} || !line.depth_test) {
            std::cerr << "AABB line attributes were not preserved.\n";
            return 1;
        }
    }

    frame.debug_lines.clear();
    debugRenderer.drawTransformAxes(glm::mat4{1.0f}, 1.0f, false);
    if (frame.debug_lines.size() != 3) {
        std::cerr << "Transform axes should emit 3 debug lines.\n";
        return 1;
    }

    debugRenderer.endFrame();
    std::cout << "DebugRenderer records line and AABB commands.\n";
    return 0;
}
