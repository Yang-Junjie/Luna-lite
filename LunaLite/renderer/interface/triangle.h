#pragma once
#include "types.h"

#include <glm/glm.hpp>

namespace lunalite::renderer::interface {
class Triangle {
public:
    Triangle(Vertex v1, Vertex v2, Vertex v3)
        : m_v1(v1),
          m_v2(v2),
          m_v3(v3)
    {}

    const Vertex& getV1() const
    {
        return m_v1;
    }

    const Vertex& getV2() const
    {
        return m_v2;
    }

    const Vertex& getV3() const
    {
        return m_v3;
    }

private:
    Vertex m_v1;
    Vertex m_v2;
    Vertex m_v3;
};
} // namespace lunalite::renderer::interface
