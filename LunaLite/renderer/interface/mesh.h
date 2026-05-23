#pragma once
#include "../../asset/asset.h"
#include "types.h"

#include <cstdint>

#include <utility>
#include <vector>

namespace lunalite::renderer::interface {
class Mesh : public asset::Asset {
public:
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

    asset::AssetType getAssetsType() const override
    {
        return asset::AssetType::Mesh;
    }

private:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    uint64_t m_vertex_version{1};
    uint64_t m_index_version{1};
};
} // namespace lunalite::renderer::interface
