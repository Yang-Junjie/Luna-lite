#pragma once
#include "asset.h"

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace lunalite::asset {
struct PrefabNode {
    std::string name;
    glm::mat4 transform{1.0f};
    AssetHandle mesh{0};
    std::vector<AssetHandle> materials;
    uint32_t submesh_start{0};
    uint32_t submesh_count{std::numeric_limits<uint32_t>::max()};
    std::vector<uint32_t> children;
};

class Prefab : public Asset {
public:
    const std::vector<PrefabNode>& getNodes() const
    {
        return m_nodes;
    }

    std::vector<PrefabNode>& editNodes()
    {
        return m_nodes;
    }

    void setNodes(std::vector<PrefabNode> nodes)
    {
        m_nodes = std::move(nodes);
    }

    const std::vector<uint32_t>& getRoots() const
    {
        return m_roots;
    }

    std::vector<uint32_t>& editRoots()
    {
        return m_roots;
    }

    void setRoots(std::vector<uint32_t> roots)
    {
        m_roots = std::move(roots);
    }

    AssetType getAssetsType() const override
    {
        return AssetType::Prefab;
    }

private:
    std::vector<PrefabNode> m_nodes;
    std::vector<uint32_t> m_roots;
};
} // namespace lunalite::asset
