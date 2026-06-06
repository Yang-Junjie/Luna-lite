#pragma once
#include "../../asset/asset.h"
#include "aabb.h"
#include "types.h"

#include <cstdint>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace lunalite::renderer::interface {
struct SubMesh {
    std::string name;
    uint32_t material_slot{0};

    const std::vector<Vertex>& getVertices() const
    {
        return m_vertices;
    }

    const std::vector<uint32_t>& getIndices() const
    {
        return m_indices;
    }

    void setVertices(std::vector<Vertex> vertices)
    {
        m_vertices = std::move(vertices);
        markVerticesDirty();
    }

    void setIndices(std::vector<uint32_t> indices)
    {
        m_indices = std::move(indices);
        markIndicesDirty();
    }

    std::vector<Vertex>& editVertices()
    {
        markVerticesDirty();
        return m_vertices;
    }

    std::vector<uint32_t>& editIndices()
    {
        markIndicesDirty();
        return m_indices;
    }

    void markVerticesDirty()
    {
        ++m_vertex_version;
        if (m_vertex_version == 0) {
            m_vertex_version = 1;
        }
    }

    void markIndicesDirty()
    {
        ++m_index_version;
        if (m_index_version == 0) {
            m_index_version = 1;
        }
    }

    uint64_t getVertexVersion() const
    {
        return m_vertex_version;
    }

    uint64_t getIndexVersion() const
    {
        return m_index_version;
    }

    const AABB& getLocalAABB() const
    {
        if (m_aabb_vertex_version != m_vertex_version) {
            recomputeLocalAABB();
        }

        return m_local_aabb;
    }

private:
    void recomputeLocalAABB() const
    {
        m_local_aabb = AABB{};
        for (const auto& vertex : m_vertices) {
            m_local_aabb.include(vertex.position);
        }
        m_aabb_vertex_version = m_vertex_version;
    }

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    uint64_t m_vertex_version{1};
    uint64_t m_index_version{1};
    mutable AABB m_local_aabb{};
    mutable uint64_t m_aabb_vertex_version{0};
};

class Mesh : public asset::Asset {
public:
    const std::vector<SubMesh>& getSubMeshes() const
    {
        return m_submeshes;
    }

    void setSubMeshes(std::vector<SubMesh> submeshes)
    {
        m_submeshes = std::move(submeshes);
    }

    std::vector<SubMesh>& editSubMeshes()
    {
        return m_submeshes;
    }

    AABB getLocalAABB(uint32_t submesh_start = 0, uint32_t submesh_count = std::numeric_limits<uint32_t>::max()) const
    {
        AABB result;
        if (submesh_start >= m_submeshes.size()) {
            return result;
        }

        const auto submeshStart = static_cast<size_t>(submesh_start);
        const auto availableSubmeshes = m_submeshes.size() - submeshStart;
        const auto requestedSubmeshes = submesh_count == std::numeric_limits<uint32_t>::max()
                                            ? availableSubmeshes
                                            : static_cast<size_t>(submesh_count);
        const auto submeshEnd = submeshStart + std::min(availableSubmeshes, requestedSubmeshes);
        for (size_t submeshIndex = submeshStart; submeshIndex < submeshEnd; ++submeshIndex) {
            result.include(m_submeshes[submeshIndex].getLocalAABB());
        }
        return result;
    }

    asset::AssetType getAssetsType() const override
    {
        return asset::AssetType::Mesh;
    }

private:
    std::vector<SubMesh> m_submeshes;
};

} // namespace lunalite::renderer::interface
