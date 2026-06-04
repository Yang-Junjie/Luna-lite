#pragma once
#include "../../asset/asset.h"

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <limits>
#include <utility>
#include <vector>

namespace lunalite::renderer::interface {
struct ModelMesh {
    asset::AssetHandle mesh{0};
    std::vector<asset::AssetHandle> materials;
    glm::mat4 transform{1.0f};
    uint32_t submesh_start{0};
    uint32_t submesh_count{std::numeric_limits<uint32_t>::max()};
};

class Model : public asset::Asset {
public:
    const std::vector<ModelMesh>& getMeshes() const
    {
        return m_meshes;
    }

    void setMeshes(std::vector<ModelMesh> meshes)
    {
        m_meshes = std::move(meshes);
    }

    std::vector<ModelMesh>& editMeshes()
    {
        return m_meshes;
    }

    asset::AssetType getAssetsType() const override
    {
        return asset::AssetType::Model;
    }

private:
    std::vector<ModelMesh> m_meshes;
};
} // namespace lunalite::renderer::interface
