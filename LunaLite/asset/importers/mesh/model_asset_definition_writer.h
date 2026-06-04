#pragma once
#include "../../asset.h"

#include <cstdint>

#include <filesystem>
#include <glm/mat4x4.hpp>
#include <limits>
#include <vector>

namespace lunalite::asset {

struct ModelMeshDefinition {
    AssetHandle mesh{0};
    std::vector<AssetHandle> materials;
    glm::mat4 transform{1.0f};
    uint32_t submesh_start{0};
    uint32_t submesh_count{std::numeric_limits<uint32_t>::max()};
};

class ModelAssetDefinitionWriter final {
public:
    void writeDefinition(const std::filesystem::path& modelPath,
                         const std::vector<ModelMeshDefinition>& meshes,
                         bool overwriteExisting = false) const;
    void writeSingleMeshDefinition(const std::filesystem::path& modelPath,
                                   AssetHandle meshHandle,
                                   const std::vector<AssetHandle>& materialHandles) const;
};

} // namespace lunalite::asset
