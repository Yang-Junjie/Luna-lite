#include "../LunaLite/renderer/interface/frustum.h"

#include <glm/glm.hpp>
#include <iostream>

namespace {
lunalite::renderer::interface::AABB makeAABB(const glm::vec3& min, const glm::vec3& max)
{
    lunalite::renderer::interface::AABB aabb;
    aabb.include(min);
    aabb.include(max);
    return aabb;
}
} // namespace

int main()
{
    using namespace lunalite;

    const auto frustum = renderer::interface::Frustum::fromViewProjection(glm::mat4{1.0f});

    if (!frustum.intersects(makeAABB({-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}))) {
        std::cerr << "AABB inside clip space should intersect frustum.\n";
        return 1;
    }

    if (!frustum.intersects(makeAABB({0.5f, -0.5f, -0.5f}, {2.0f, 0.5f, 0.5f}))) {
        std::cerr << "AABB crossing the frustum boundary should intersect frustum.\n";
        return 1;
    }

    if (frustum.intersects(makeAABB({1.1f, -0.5f, -0.5f}, {2.0f, 0.5f, 0.5f}))) {
        std::cerr << "AABB outside the right frustum plane should not intersect frustum.\n";
        return 1;
    }

    if (!frustum.intersects(renderer::interface::AABB{})) {
        std::cerr << "Invalid AABB should be treated conservatively as visible.\n";
        return 1;
    }

    std::cout << "Frustum intersects AABB bounds conservatively.\n";
    return 0;
}
