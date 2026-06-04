#pragma once
#include "types.h"

#include <cmath>
#include <cstdint>

#include <glm/geometric.hpp>
#include <vector>

namespace lunalite::renderer::interface {
namespace detail {
inline bool validVector(const glm::vec3& value)
{
    const float lengthSq = glm::dot(value, value);
    return std::isfinite(lengthSq) && lengthSq > 1e-12f;
}

inline glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback)
{
    return validVector(value) ? glm::normalize(value) : fallback;
}

inline glm::vec3 fallbackTangent(const glm::vec3& normal)
{
    const glm::vec3 axis = std::abs(normal.y) < 0.999f ? glm::vec3{0.0f, 1.0f, 0.0f} : glm::vec3{1.0f, 0.0f, 0.0f};
    return safeNormalize(glm::cross(axis, normal), glm::vec3{1.0f, 0.0f, 0.0f});
}
} // namespace detail

inline void orthonormalizeTangentBasis(Vertex& vertex)
{
    const glm::vec3 normal = detail::safeNormalize(vertex.normal, glm::vec3{0.0f, 1.0f, 0.0f});
    glm::vec3 tangent = vertex.tangent - normal * glm::dot(normal, vertex.tangent);
    tangent = detail::validVector(tangent) ? glm::normalize(tangent) : detail::fallbackTangent(normal);

    const glm::vec3 referenceBitangent = glm::cross(normal, tangent);
    const float handedness =
        detail::validVector(vertex.bitangent) && glm::dot(referenceBitangent, vertex.bitangent) < 0.0f ? -1.0f : 1.0f;

    vertex.normal = normal;
    vertex.tangent = tangent;
    vertex.bitangent = detail::safeNormalize(referenceBitangent * handedness, glm::cross(normal, tangent));
}

inline void orthonormalizeTangentBasis(std::vector<Vertex>& vertices)
{
    for (auto& vertex : vertices) {
        orthonormalizeTangentBasis(vertex);
    }
}

inline void generateTangentBasis(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    if (vertices.empty()) {
        return;
    }

    std::vector<glm::vec3> tangents(vertices.size(), glm::vec3{0.0f});
    std::vector<glm::vec3> bitangents(vertices.size(), glm::vec3{0.0f});

    const size_t indexCount = indices.empty() ? vertices.size() : indices.size();
    for (size_t i = 0; i + 2 < indexCount; i += 3) {
        const uint32_t i0 = indices.empty() ? static_cast<uint32_t>(i + 0) : indices[i + 0];
        const uint32_t i1 = indices.empty() ? static_cast<uint32_t>(i + 1) : indices[i + 1];
        const uint32_t i2 = indices.empty() ? static_cast<uint32_t>(i + 2) : indices[i + 2];
        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
            continue;
        }

        const auto& v0 = vertices[i0];
        const auto& v1 = vertices[i1];
        const auto& v2 = vertices[i2];

        const glm::vec3 edge1 = v1.position - v0.position;
        const glm::vec3 edge2 = v2.position - v0.position;
        const glm::vec2 deltaUV1 = v1.uv - v0.uv;
        const glm::vec2 deltaUV2 = v2.uv - v0.uv;

        const float determinant = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (std::abs(determinant) < 1e-8f) {
            continue;
        }

        const float invDeterminant = 1.0f / determinant;
        const glm::vec3 tangent = (edge1 * deltaUV2.y - edge2 * deltaUV1.y) * invDeterminant;
        const glm::vec3 bitangent = (edge2 * deltaUV1.x - edge1 * deltaUV2.x) * invDeterminant;
        if (!detail::validVector(tangent) || !detail::validVector(bitangent)) {
            continue;
        }

        tangents[i0] += tangent;
        tangents[i1] += tangent;
        tangents[i2] += tangent;
        bitangents[i0] += bitangent;
        bitangents[i1] += bitangent;
        bitangents[i2] += bitangent;
    }

    for (size_t i = 0; i < vertices.size(); ++i) {
        vertices[i].tangent = tangents[i];
        vertices[i].bitangent = bitangents[i];
        orthonormalizeTangentBasis(vertices[i]);
    }
}
} // namespace lunalite::renderer::interface
