#pragma once
#include "../renderer/interface/triangle.h"

#include <vector>

namespace lunalite::scene {
class Scene {
public:
    Scene() = default;
    void addTriangle(renderer::interface::Triangle triangle);

    const std::vector<renderer::interface::Triangle>& getTriangles() const
    {
        return m_triangles;
    }

private:
    std::vector<renderer::interface::Triangle> m_triangles;
};
} // namespace lunalite::scene
