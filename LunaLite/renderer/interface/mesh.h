#pragma once
#include "../../asset/asset.h"
#include "types.h"

#include <cstdint>

#include <vector>

namespace lunalite::renderer::interface {
class Mesh : public asset::Asset {
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    asset::AssetType getAssetsType() const override
    {
        return asset::AssetType::Mesh;
    }
};
} // namespace lunalite::renderer::interface
