#pragma once
#include "../../asset/asset.h"

#include <utility>
#include <vector>

namespace lunalite::renderer::interface {
struct ModelMesh {
    asset::AssetHandle mesh{0};
    std::vector<asset::AssetHandle> materials;
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
